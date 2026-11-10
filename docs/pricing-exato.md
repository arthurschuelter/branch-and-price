# Funcionamento de `Pricer::exact()`

Esta explicação descreve a implementação atual em
[`src/core/pricing.cpp`](../src/core/pricing.cpp). Ela é específica do problema
de roteamento com prioridades e conflitos.

## 1. O que a função procura

Para cada dia `d`, a função procura a rota viável de menor custo reduzido,
respeitando capacidade, conflitos e decisões de branching do nó. Para uma
rota `R`, o custo reduzido é:

$$
\bar c(R,d) = \rho(R) + \sum_{i\in R}\pi(i,d)
               - \sum_{i\in R}u_i - \lambda_d.
$$

Aqui, `rho(R)` é o custo de deslocamento, `pi(i,d)` é a prioridade do cliente
no dia, `u_i` é o dual de cobertura do cliente e `lambda_d` é o dual do dia.
Se o custo reduzido for menor que `-1e-6`, a função devolve a rota como coluna
candidata. É retornada no máximo uma rota por dia: a melhor do modelo daquele
dia. Se não houver rota melhorante no dia, a função passa ao próximo.

## 2. Entradas e validações iniciais

- `customerDuals`: duais das restrições de cobertura dos clientes.
- `dayDuals`: duais das restrições dos dias.
- `decisions`: decisões SAME e DIFFERENT acumuladas até o nó.
- `remainingSeconds`: tempo disponível para a chamada.

Se o tempo recebido for zero ou negativo, a função retorna `Interrupted`. Em
seguida, inicia o relógio e verifica o tamanho e a finitude dos dois vetores de
duais. Se forem inválidos, retorna `Error`. As posições do depósito e do dia
zero existem nos vetores, mas não são usadas no objetivo.

## 3. Variáveis do modelo

A função cria um ambiente e um modelo Gurobi com as seguintes variáveis:

| Variável | Tipo | Significado |
| --- | --- | --- |
| `y[i]` | Binária | Cliente `i` participa da rota. |
| `x[i][j]` | Binária | A rota percorre o arco `i → j`. |
| `flow[i][j]` | Contínua | Quantidade de fluxo enviada pelo arco `i → j`. |

Não existe variável `y` para o depósito, nem arcos de um vértice para ele mesmo.
Os parâmetros `MIPGap` e `MIPGapAbs` são definidos como zero para não aceitar
os gaps de parada padrão; tolerâncias numéricas do solver ainda se aplicam.

## 4. Fluxo e eliminação de subciclos

O modelo permite fluxo apenas em arcos selecionados:

$$flow_{ij} \le (n-1)x_{ij}.$$

Se `x[i][j]` é zero, o fluxo também é zero. O limite `n - 1` corresponde ao
número máximo de clientes da rota. Para cada cliente, o balanço é:

$$\sum_j flow_{ji} - \sum_j flow_{ij} = y_i.$$

Cada cliente selecionado consome uma unidade líquida de fluxo. Por exemplo,
na rota `depósito → A → B → depósito`, o depósito pode enviar duas unidades
para A; A consome uma e envia a outra para B.

Um ciclo desconectado como `C → D → C` precisaria receber fluxo de fora para
atender ao consumo dos dois clientes. Como não tem arco selecionado ligando-o
ao depósito, ele não consegue satisfazer os balanços. A formulação ainda deve
ser validada em uma variedade maior de instâncias.

## 5. Restrições da rota

Para cada cliente `i`, o número de arcos que entram e o número de arcos que
saem são iguais a `y[i]`. Assim, um cliente selecionado tem uma entrada e uma
saída; um cliente não selecionado não usa esses arcos.

No depósito, o modelo exige uma saída e uma entrada. As demais restrições são:

- **Capacidade:** a soma dos pesos dos clientes selecionados não excede
  `inst_.capacity`.
- **Conflitos:** para cada par conflitante de clientes, `y[i] + y[j] <= 1`.
- **SAME:** `y[i] == y[j]`, exigindo selecionar ambos ou nenhum.
- **DIFFERENT:** `y[i] + y[j] <= 1`, impedindo selecionar os dois.

Antes de aplicar as decisões, a função valida seus índices e rejeita o
depósito e pares com o mesmo cliente.

## 6. Objetivo e resolução por dia

As restrições são comuns aos dias. Para cada dia `d`, o código monta:

$$
\min \sum_{i\ne j} c_{ij}x_{ij}
 + \sum_{i\ne depósito}(\pi(i,d)-u_i)y_i - \lambda_d.
$$

O primeiro termo é o custo dos arcos. O segundo soma prioridades e desconta
os duais dos clientes selecionados. O último desconta o dual do dia.

Antes de cada `optimize()`, a função calcula o tempo restante desde o início
da chamada e configura o `TimeLimit` do Gurobi. Se o tempo acabar, retorna
`Interrupted`, preservando as colunas encontradas em dias anteriores.

Após a resolução:

- `GRB_TIME_LIMIT` resulta em `Interrupted`.
- `GRB_INFEASIBLE` leva ao próximo dia.
- Status diferente de `GRB_OPTIMAL` resulta em `Error`.
- Se `ObjVal >= -eps`, a função consulta `ObjBound`. Um limite não finito ou
  menor que `-eps` resulta em `Inconclusive`; caso contrário, ela passa ao próximo dia.

O limite inconclusivo mantém o nó aberto. Ainda falta revisar as tolerâncias do solver.

## 7. Reconstrução e validação da coluna

Quando o objetivo é menor que `-eps`, a função começa pelo depósito e segue os
arcos com `x[current][j] > 0.5` até reencontrá-lo. Rejeita ausência de próximo
vértice, múltiplas saídas ou cliente repetido. Depois confere se os clientes
encontrados correspondem às variáveis `y`.

Em seguida:

1. Chama `evaluateRoute()` para validar a rota e recalcular seu custo.
2. Confere as decisões SAME/DIFFERENT.
3. Preenche `Column.cost`.
4. Recalcula o custo reduzido e o compara com o objetivo do Gurobi.
5. Guarda a coluna se as verificações passarem.

## 8. Resultado e limitações atuais

Após processar todos os dias, a função retorna `PricingStatus::Completed` e as
colunas encontradas, que podem ser nenhuma. Uma busca concluída sem colunas
indica ausência de custo reduzido abaixo de `-eps` segundo os estados, limites
e tolerâncias aceitos pela implementação.

O ciclo de geração de colunas chama a heurística e, quando necessário, o pricing
exato. Ainda é necessário revisar tolerâncias e ampliar a validação em instâncias
pequenas e sob interrupção. O resultado ainda não atualiza o LB nem fecha o nó.
