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
- [ ] Conferir que o exemplo oficial gera 36 colunas unitárias (6 clientes × 6 dias).

## Próximos passos: controle da busca

- [ ] Implementar `selectNode()` com DFS: mover o último nó de `open_`, removê-lo da deque e retorná-lo.
- [ ] Implementar `timeIsUp()` comparando o tempo decorrido desde `t0_` com `cfg_.time_limit`.

## Depois: resolver a raiz com geração de colunas

- [ ] Implementar o `RMPSolver` como LP: cada cliente deve ser atendido exatamente uma vez e cada dia pode ter no máximo uma rota.
- [ ] Disponibilizar objetivo, valores das variáveis e duais dos clientes e dos dias no `RMPSolver`.
- [ ] Criar `rmp_` e `pricer_` no construtor de `BranchAndPrice`.
- [ ] Implementar `processNode()` para montar o PMR com as colunas permitidas e garantir viabilidade, usando variáveis artificiais quando necessário.
- [ ] Montar o PMR da raiz com as colunas iniciais e resolver o LP.
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

- [ ] Implementar a heurística de arredondamento com o pool de colunas e limite `cfg_.heuristic_time`.
- [ ] Preencher estatísticas de colunas, pricing e tempo total, além dos registros de execução.
- [ ] Definir e implementar o pré-processamento/propagação da EXP2, condicionado a `cfg_.use_preprocessing` (adiado por enquanto).
