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

## Próximo passo: inicializar o nó raiz (`initRoot`)

- [ ] Para cada cliente `i` e dia `d`, criar uma coluna unitária com `route = {i}`.
- [ ] Preencher `has` para indicar o cliente presente na coluna.
- [ ] Obter `Column.cost` com `evaluateRoute({i}, d).total_cost`.
- [ ] Adicionar as colunas ao pool global `pool_`.
- [ ] Criar o nó raiz (`id = 0`, `depth = 0`, sem decisões e `lb = -inf`).
- [ ] Colocar o nó raiz em `open_`.
- [ ] Conferir que o exemplo oficial gera 36 colunas unitárias (6 clientes × 6 dias).

## Depois: mestre restrito na raiz

- [ ] Implementar o `RMPSolver` como LP: cada cliente deve ser atendido exatamente uma vez e cada dia pode ter no máximo uma rota.
- [ ] Montar o PMR da raiz com as colunas iniciais e resolver o LP.
- [ ] Extrair os duais dos clientes e dos dias.
- [ ] Implementar pricing heurístico seguido de pricing exato quando a heurística não encontrar coluna de custo reduzido negativo.
- [ ] Repetir o ciclo até o pricing exato provar que não há coluna com custo reduzido negativo; então registrar o limite inferior da raiz.
