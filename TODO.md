# TODO

## Instância, parser e avaliação de rota

- [x] Seguir o formato da Seção 10.2 do `enunciado.pdf`.
- [x] Ignorar comentários iniciados por `#`.
- [x] Ler prioridades por dia, custos de deslocamento `c(i,j)` e conflitos.
- [x] Validar campos, índices, custos, conflitos e conteúdo extra.
- [x] Usar o exemplo oficial da Seção 8 em `tests/instance_zero.txt`.
- [x] Executar o parser e conferir `evaluateRoute` com o exemplo oficial.
- [x] Validar dia, sequência de clientes, capacidade e conflitos em `evaluateRoute`.
- [x] Calcular custo logístico pela matriz `c` na ordem da rota, incluindo ida e volta ao depósito.
- [x] Somar as prioridades do dia e preencher `RouteEvaluation`.

## Inicialização do nó raiz (`initRoot`)

- [x] Para cada cliente `i` e dia `d`, criar uma coluna unitária com `route = {i}`, ignorando o depósito configurado.
- [x] Preencher `has` para indicar o cliente presente na coluna e atribuir o dia.
- [x] Avaliar a rota, descartar avaliações inválidas e obter `Column.cost` com `evaluateRoute({i}, d).total_cost`.
- [x] Limpar `pool_` e `open_` antes da inicialização e adicionar as colunas ao pool global.
- [x] Criar o nó raiz (`id = 0`, `depth = 0`, sem decisões e `lb = -inf`).
- [x] Implementar `pushNode()` e colocar o nó raiz em `open_` transferindo a propriedade com `std::move`.
- [x] Conferir que o exemplo oficial gera 36 colunas unitárias (6 clientes × 6 dias).

## Controle da busca (concluído)

- [x] Implementar `selectNode()` com DFS: mover `open_.back()` com `std::move`, remover com `pop_back()` e retornar o nó; inserção com `push_back()` mantém a ordem de pilha.
- [x] Implementar `timeIsUp()` usando `steady_clock` e `duration<double>` para comparar segundos decorridos com `cfg_.time_limit` usando `>=`; inicializar `t0_` no começo de `solve()`.

## PMR inicial da raiz — progresso de 07/11/2026

Implementação e validação inicial da Fase 2.1 do `roadmap.md`, usando apenas as colunas unitárias disponíveis.

- [x] Implementar o construtor de `RMPSolver` e `reset()`: limpar variáveis e restrições anteriores, recriar o modelo de minimização, criar cobertura para clientes (sem depósito) e restrições para os dias `1..n-1`.
- [x] Implementar `addColumn()` com variáveis contínuas não negativas, custo `Column.cost`, coeficientes de cobertura e um coeficiente para o dia da rota; guardar as variáveis em `vars_` na ordem de inserção.
- [x] Implementar `solveLP(remainingSeconds)`: não otimizar sem tempo, configurar o limite e retornar o status do Gurobi.
- [x] Implementar `objective()`, `values()`, `customerDuals()` e `dayDuals()`; preservar zero nos índices não utilizados dos vetores de duais.
- [x] Criar um `RMPSolver` local em `solve()`, após `initRoot()`, adicionar as colunas de `pool_` e resolver com o tempo restante calculado desde `t0_`.
- [x] Consultar resultados apenas após `GRB_OPTIMAL` e conferir um valor de `x` por coluna.
- [x] Implementar as verificações do TODO 7: cobertura igual a 1 por cliente, uso de cada dia no máximo 1 e objetivo recalculado como soma de `pool_[k].cost * x[k]`, com tolerância de `1e-6`.
- [x] Compilar com `cmake --build build` e executar o exemplo oficial com depósito 0, capacidade 10 e limite de 60 segundos: 36 colunas, 12 restrições, objetivo **78**, sem falhas nas verificações.
- [x] Imprimir no `main.cpp` o custo da solução retornada e, por coluna selecionada, dia, clientes e custo.
- [x] Registrar o resumo da sessão em `ai/prompt-005.md` (criado antes da implementação final dos itens 7d–7f).

O teste com Gurobi precisou ser executado fora do sandbox para reconhecer o HostID da licença. A compilação passou com avisos em trechos ainda incompletos. O resultado 78 corresponde ao PMR com rotas unitárias; não é o ótimo 59 do problema completo nem um LB certificado. A execução observada selecionou os clientes 1, 3, 5, 6, 4 e 2 nos dias 1 a 6, respectivamente.

## Próxima sessão: concluir inspeção e integrar o PMR

- [ ] Consultar e inspecionar os duais dos clientes e dos dias no exemplo oficial; os métodos estão implementados, mas essa inspeção ainda não foi realizada.
- [ ] Criar `rmp_` no construtor de `BranchAndPrice` e integrar seu uso ao processamento dos nós; atualmente a validação usa um objeto local em `solve()`.
- [ ] Substituir o retorno antecipado da validação em `solve()` ao integrar a busca: atualmente `return solution` encerra a execução antes do `while` quando o PMR inicial é ótimo.
- [ ] Verificar integralidade antes de converter `x` em `Solution`; selecionar `x[k] > 0.5` por si só não valida uma solução inteira geral.
- [ ] Validar a `Solution` construída: cobertura, dias, capacidade, conflitos e custo recalculado das rotas; as verificações atuais do TODO 7 conferem a solução LP antes dessa conversão.
- [ ] Tratar explicitamente resultados diferentes de `GRB_OPTIMAL`, mantendo incumbente viável e LB certificado sob interrupção.

## Depois: completar a geração de colunas na raiz

- [ ] Criar `pricer_` no construtor de `BranchAndPrice`.
- [ ] Implementar `processNode()` para montar o PMR com as colunas permitidas e garantir viabilidade conforme a Fase 3.1 do roadmap; se optar por artificiais, usar uma fase de viabilidade certificada.
- [ ] Remover o `return true` provisório no início de `columnGeneration()` ao implementar o ciclo.
- [ ] Implementar pricing heurístico seguido de pricing exato quando a heurística não encontrar coluna de custo reduzido negativo.
- [ ] Implementar `columnGeneration()`: resolver o LP, extrair duais, executar pricing e adicionar novas colunas sem duplicação.
- [ ] Repetir o ciclo até o pricing exato provar que não há coluna com custo reduzido negativo; verificar as artificiais e registrar o limite inferior da raiz.
- [ ] Tratar interrupção por tempo sem usar o objetivo de um PMR não convergido como limite inferior válido.

## Depois: soluções inteiras e ramificação

- [ ] Implementar `isIntegral()` e montar a solução a partir das variáveis do mestre.
- [ ] Implementar `updateIncumbent()` para guardar soluções de menor custo.
- [ ] Implementar `columnAllowed()` respeitando decisões SAME e DIFFERENT.
- [ ] Implementar `selectBranchingPair()` e criar os filhos em `branch()` com decisões de Ryan-Foster.
- [ ] Implementar podas por limitante antes e depois do processamento de cada nó.
- [ ] Adicionar seleção por melhor limitante em `selectNode()`.
- [ ] Implementar `updateGlobalLB()` considerando os nós ainda não resolvidos.

## Etapas posteriores

- [ ] Gravar a solução e o LB no formato oficial em `-o`; a impressão atual no terminal é apenas para inspeção.
- [ ] Implementar a heurística de arredondamento com o pool de colunas e limite `cfg_.heuristic_time`.
- [ ] Preencher estatísticas de colunas, pricing e tempo total, além dos registros de execução.
- [ ] Definir e implementar o pré-processamento/propagação da EXP2, condicionado a `cfg_.use_preprocessing` (adiado por enquanto).
