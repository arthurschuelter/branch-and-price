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

O `Pricer` cria o ambiente Gurobi na primeira chamada exata e o reutiliza nas
chamadas seguintes. Cada chamada monta um novo modelo, com as decisões do nó
atual; dentro da chamada, o modelo é reutilizado para resolver cada dia. Isso
evita carregar restrições de um nó para outro. O modelo possui estas variáveis:

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
pequenas e sob interrupção. Os limites `ObjBound` não atualizam o LB nem fecham
o nó. A enumeração independente descrita na seção 9 pode fazê-lo em instâncias
pequenas, quando suas condições são satisfeitas.

`PricingResult.dayLowerBounds` registra `ObjBound` finito por dia ótimo; dias não
processados ou sem limite finito ficam como `nullopt`, e dias comprovadamente
inviáveis recebem zero, pois não têm coluna admissível. Uma interrupção ou
conclusão inconclusiva preserva os limites já obtidos, sem usá-los como LB.

Quando o pricing termina sem candidatas e todos os limites estão disponíveis, o
ciclo imprime apenas uma candidata de diagnóstico: soma dos duais de cobertura,
mais `min(lambda[d], 0) + min(0, bound[d] - eps)` para cada dia. A ideia é
reduzir os duais de dia para compensar possíveis custos reduzidos negativos.
O cálculo está em `diagnosticLowerBound()`, que retorna `nullopt` quando o pricing
não terminou sem colunas ou quando dimensões ou valores necessários são inválidos.
O diagnóstico é impresso com precisão suficiente para distinguir valores `double`.
O valor não é certificado: precisão de `ObjBound`, viabilidade e arredondamento
dos duais e a margem `eps` ainda exigem revisão. Não é usado para poda ou fechamento.

## 9. Limite inteiro por enumeração independente

Após o pricing exato terminar sem candidatas, para instâncias com até oito
clientes, `enumerateCertificate()` percorre todas as sequências viáveis,
respeitando capacidade, conflitos e decisões do nó. Para cada dia, calcula o
menor custo reduzido usando duais de cobertura quantizados em múltiplos de
`1/1024`, com aritmética inteira. Os duais são limitados antes da quantização;
isso é permitido porque não precisam ser ótimos para construir uma cota dual.

Para cada dia, o dual de dia escolhido é o menor entre zero e o menor custo
reduzido enumerado com dual de dia zero. Assim, todas as colunas admissíveis
obedecem às restrições duais. A soma desses duais fornece uma cota para o LP;
como o objetivo inteiro de roteamento é inteiro, o teto dessa soma racional é
um LB válido para o problema inteiro. O teto é calculado em aritmética inteira.

Se a enumeração for interrompida, houver mais de oito clientes ou as entradas
necessárias forem inválidas, ela não retorna cota nova. O LB herdado é mantido.
O procedimento também tenta reconstruir uma solução primal racional do PMR com
denominador 1024: ela deve respeitar exatamente cobertura e dias, ter custos e
colunas válidos e estar próxima dos valores obtidos no PMR. Se o objetivo dessa
solução for exatamente igual, em aritmética inteira, ao objetivo dual construído
pela enumeração de todas as rotas, a igualdade primal-dual certifica o ótimo do
LP completo. Só nesse caso o nó retorna `Converged` e pode ser ramificado.

Se não for possível reconstruir a solução ou demonstrar a igualdade, a cota
inteira ainda pode podar o nó quando atinge a incumbente; caso contrário, o nó
fica aberto. Essa certificação é limitada a oito clientes e à possibilidade de
representar a solução primal e o dual ótimos com a escala escolhida. Não usa a
candidata numérica baseada em `ObjBound` para certificar o LP.

### Fundamentação e limites da revisão

Para cada dia, seja `m[d]` o mínimo entre zero e os custos reduzidos de
**todas** as rotas admissíveis, calculados com os duais de cliente quantizados
e dual de dia zero. Então `m[d] <= 0` e, para cada rota, a soma de seus duais
de cliente mais `m[d]` não excede seu custo. Os duais construídos são, portanto,
viáveis para o mestre completo. Sua soma é um LB do LP; como os custos são
inteiros, seu teto exato é um LB do problema inteiro. A igualdade exata com
uma solução primal viável prova também a convergência do LP. Interromper a
enumeração impede usar o mínimo parcial, que pode ser maior que o verdadeiro.

Essa justificativa depende de enumerar corretamente as rotas e de aritmética
sem transbordamento. O limite de oito clientes e os dados inteiros de entrada
limitam as somas usadas pela enumeração; o código rejeita transbordamento na
soma primal. A revisão é por inspeção e não substitui comparação independente
em instâncias variadas, sobretudo com decisões de ramificação e interrupções.

Para instâncias gerais, a mesma construção dual seria válida se cada limite
por dia fosse uma cota inferior matematicamente garantida do custo reduzido e
as operações fossem arredondadas para baixo. O `ObjBound` e a margem fixa
atualmente adotada não demonstram, por si, essa garantia para todas as escalas
e tolerâncias do solver. Até fundamentar esses erros e a conversão dos dados,
a candidata continua restrita ao diagnóstico; não prova convergência nem LB.

## 10. Comparação manual com enumeração independente

Nesta sessão, uma chamada direta ao `Pricer::exact()` foi comparada com um
enumerador temporário separado, que percorreu permutações de todos os subconjuntos
não vazios e calculou custos e restrições a partir dos dados abaixo. Ambos foram
executados localmente com Gurobi 13.0.2; nenhum teste automatizado foi adicionado
ao projeto. O depósito foi 2, pesos dos clientes 1, capacidade 4, sem conflitos
salvo indicação na tabela. Cada cliente tem prioridades `(0,1,2,3)`, os duais
de clientes são `(8,8,0,8,0)` nas posições 0 a 4 e os duais de dia são zero.
A matriz simétrica de custos, nas mesmas posições, é:

```
0  0 10  0 15
0  0 10  0 10
10 10 0 10  1
0  0 10  0 10
15 10 1 10  0
```

| Caso | Decisões ou alteração | Mínimos enumerados por dia | ObjBound observado por dia | Rotas melhorantes retornadas |
|---|---|---|---|---|
| Base | Nenhuma | `-4, -1, 2, 5` | `-4, -1, 2, 5` | Dias 1 e 2: `(1,3,0)`, custos reduzidos `-4` e `-1` |
| Conflito | `(0,3)` | `2, 3, 4, 5` | `2, 3, 4, 5` | Nenhuma |
| SAME | `(0,4)` | `-3, 1, 5, 9` | `-3, 1, 5, 9` | Dia 1: `(4,1,3,0)`, custo reduzido `-3` |
| DIFFERENT | `(0,1)` | `2, 3, 4, 5` | `2, 3, 4, 5` | Nenhuma |
| Capacidade | 2 | `2, 3, 4, 5` | `2, 3, 4, 5` | Nenhuma |

O status observado foi `Completed` em todas as chamadas. A enumeração encontrou
rotas possivelmente em ordem diferente para as colunas melhorantes; custos e conjuntos
coincidem. Para mínimos não negativos, o pricing não retorna rota, logo a comparação
foi com o `ObjBound` observado, não com uma rota reconstruída pelo pricing.

O caso base também verifica uma situação propícia a subciclo: sem conexão por
fluxo, o ciclo `2-4-2` junto do ciclo desconectado `0-1-3-0` teria custo reduzido
`-22` no dia 1. O mínimo conectado enumerado e observado foi `-4`. A matriz é
não métrica: `c(0,4)=15 > c(0,1)+c(1,4)=10`.
