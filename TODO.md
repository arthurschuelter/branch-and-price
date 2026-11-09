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

O esqueleto comentado está em `src/core/branch_and_price.cpp`. Os passos 1, 2,
3 e 4 foram realizados; os passos 7 e 8 existem no diagnóstico da raiz. A
integração completa à árvore e a certificação pelo pricing continuam pendentes.

1. Concluído: inspecionar os duais do PMR, conferir objetivo dual e custos reduzidos das colunas disponíveis (`PMR 1a–1b`).
2. Concluído: criar o membro `rmp_` no construtor (`PMR 2`).
3. Concluído: montar e validar uma incumbente unitária antes da primeira otimização (`PMR 3`). No exemplo oficial com `-t 0`, retornou seis rotas unitárias, uma por dia, com custo total 83.
4. Concluído para o diagnóstico da raiz: transferir o teste de `solve()` para `processNode()` e remover o retorno antecipado (`PMR 4`). O nó ainda permanece aberto sem certificação.
5. Filtragem, mapeamento e formação de grupos SAME implementados. Decisões DIFFERENT internas são detectadas; grupos são avaliados quanto a capacidade e conflitos e recebem colunas para cada dia, sem duplicar sequência/dia. O usuário verificou a filtragem e o mapeamento, incluindo SAME, DIFFERENT e múltiplas decisões, e a formação transitiva do grupo `{1, 3, 5}`. Falta verificar os demais casos dos grupos e a viabilidade do PMR filtrado (`PMR 5b`).
6. Tempo restante implementado; `GRB_TIME_LIMIT` e `GRB_INFEASIBLE` retornam nó não resolvido. Falta verificar o LP com colunas filtradas (`PMR 6a–6c`).
7. Implementado: verificar integralidade antes de construir `Solution`; LP fracionário válido retorna `std::nullopt`. `isIntegral()` também consulta e verifica os valores do mestre. Código inspecionado e compilado; falta verificar em execução o caso fracionário e integrar a função à busca após LP ótimo e certificação (`PMR 7–7a`).
8. Implementado no diagnóstico: validar a solução inteira e atualizar a incumbente somente quando há solução (`PMR 8–8a`).
9. `NodeResult` declarado, retornado por `processNode()` e tratado em `solve()`; implementação inspecionada e compilada. O resultado de convergência ainda depende do pricing (`PMR 9`).
10. Condicionar podas e fechamento à certificação; em seguida implementar os marcadores `CG 1–6` de geração de colunas (`PMR 10`).

Conferir o PMR unitário do exemplo contra o registro 78 e as 36 colunas, sem
interpretar esse resultado como LB certificado ou ótimo completo 59. Não habilitar
a árvore como concluída enquanto `columnGeneration()` ainda retornar provisoriamente.

### Sequência de trabalho em `processNode()`

1. **PMR 5b:** verificar DIFFERENT interno, capacidade, conflitos, criação e deduplicação das colunas dos grupos e viabilidade do PMR filtrado.
2. **PMR 6a–6c:** verificar o LP com colunas filtradas. Timeout e PMR inviável preservam o nó como não resolvido; inviabilidade do PMR filtrado não certifica inviabilidade do nó.
3. **PMR 7–8:** validação primal/dual ocorre antes da construção condicional de solução inteira; o código aceita LP fracionário válido e atualiza a incumbente apenas para solução inteira validada. Verificar o comportamento fracionário após a alteração; as duas partes ainda estão na mesma função.
4. **PMR 9:** `solve()` trata os resultados atuais de `processNode()`; falta integrar a convergência certificada ao pricing e verificar os casos em execução.
5. **PMR 10:** condicionar podas e fechamento à certificação. Concluir essa etapa após implementar o ciclo de geração de colunas e o pricing exato (`CG 1–6`).

- [x] Consultar e inspecionar os duais dos clientes e dos dias no exemplo oficial; validar tamanhos, valores finitos, índices não utilizados, sinal dos duais de dia, igualdade primal/dual, custos reduzidos e complementaridade nas colunas disponíveis.
- [x] Criar `rmp_` no construtor e usá-lo no diagnóstico do PMR da raiz em `processNode()`.
- [x] Remover o retorno antecipado da validação em `solve()`; o nó não certificado é devolvido à fronteira.
- [x] Verificar integralidade de todos os valores antes de converter `x` em `Solution`; retornar `std::nullopt` para LP fracionário válido, sem atualizar a incumbente. Alteração inspecionada e compilada.
- [ ] Verificar em execução o retorno para LP fracionário após a alteração.
- [x] Validar a `Solution` construída: cobertura, dias, capacidade, conflitos e custos recalculados por `evaluateRoute`; comparar a soma com o objetivo LP e só então atualizar a incumbente.
- [x] Inspecionar o tratamento provisório de `GRB_TIME_LIMIT` e `GRB_INFEASIBLE`: o nó retorna à fronteira sem alterar incumbente ou LB.
- [x] Declarar `NodeResult` com resultados para nó não resolvido, inviável, convergido e erro; inspecionado e compilado.
- [x] Retornar `NodeResult` em `processNode()`: inviabilidade comprovada, nó não resolvido ou erro; inspeção do código, integração ainda pendente.
- [x] Tratar `NodeResult` em `solve()`: preservar nó não resolvido, descartar nó inviável e sinalizar erro; inspecionado e compilado.
- [ ] Verificar os resultados em execução e integrar convergência certificada ao pricing (PMR 9).

Validação dos blocos `GRB_OPTIMAL`: compilação com Gurobi 13.0.2 e execução do
exemplo oficial com objetivo primal/dual 78. Duais observados: clientes 1 a 6 =
`(18, 14, 19, 16, 15, 16)`; dias 1 a 6 = `(-9, -5, -3, -2, -1, 0)`.
Passaram também uma instância de custo zero com depósito 2 e o exemplo com
depósito 3 (PMR unitário 83, conferido por enumeração das atribuições aos dias).
Antes da alteração para `std::optional`, um teste temporário com substituto de `RMPSolver` verificou o caso válido e a
rejeição de oito casos: tamanho de duais, índice não utilizado, objetivo dual,
custo reduzido, complementaridade, sinal de dual de dia, variável não finita e LP
fracionário. Esses testes não validam a árvore nem o pricing ainda pendentes.
O build foi feito em `/private/tmp/branch-and-price-pmr-build`, pois o cache
existente de `build/` referencia outro caminho do repositório.

## Depois: completar a geração de colunas na raiz

- [x] Declarar `reducedCost()` na parte privada de `BranchAndPrice`; declaração inspecionada e compilada.
- [x] Implementar `reducedCost()` para coluna válida e duais indexados por cliente e dia; inspecionado e compilado.
- [x] Reutilizar `reducedCost()` no diagnóstico do PMR; compilação e execução do exemplo oficial passaram nas verificações das 36 colunas, com objetivo primal/dual 78.
- [x] Declarar `PricingStatus` e `PricingResult` para representar status e colunas do pricing; inspecionado e compilado.
- [x] Declarar a interface do `Pricer`, distinguindo pricing heurístico e exato, decisões do nó e tempo disponível; header verificado com compilação de sintaxe.
- [x] Implementar o construtor de `Pricer` e incluir `pricing.cpp` no CMake; inspecionado e compilado.
- [x] Criar esqueleto de `Pricer::exact()` com retorno provisório `Interrupted`; inspecionado e compilado.
- [ ] Implementar pricing exato para todos os dias, decisões do nó e eliminação de subciclos; certificar ausência de colunas melhorantes apenas após conclusão exata.
- [ ] Implementar os métodos do `Pricer` e integrar sua interface ao ciclo de geração de colunas.
- [x] Criar `pricer_` no construtor de `BranchAndPrice`; inspecionado e compilado.
- [x] Criar esqueleto compilável de `Pricer::heuristic()` que retorna interrupção para tempo esgotado.
- [x] Formar identificadores transitivos para decisões SAME em `Pricer::heuristic()`; inspecionado e compilado.
- [x] Montar listas dos clientes em cada grupo SAME, excluindo o depósito; inspecionado e compilado.
- [x] Detectar decisões DIFFERENT dentro de grupo SAME na heurística e sinalizar erro; inspecionado e compilado.
- [x] Percorrer grupos por dia e avaliar rotas iniciais quanto a capacidade e conflitos; inspecionado e compilado.
- [x] Montar colunas iniciais de grupos com rota, dia, custo e vetor de presença coerentes; inspecionado e compilado. Ainda não são retornadas.
- [x] Construir candidatas combinando dois grupos, verificar capacidade, conflitos e decisões DIFFERENT e calcular custo reduzido; inspecionado e compilado.
- [x] Selecionar e guardar candidatas de custo reduzido negativo com tolerância; inspecionado e compilado.
- [x] Retornar colunas encontradas no encerramento e na interrupção; inspecionado e compilado.
- [x] Verificar tempo em cada iteração do laço de combinações; inspecionado e compilado.
- [x] Verificar tempo antes de avaliar cada grupo inicial; inspecionado e compilado.
- [x] Verificar tempo entre decisões na formação dos grupos SAME; inspecionado e compilado.
- [x] Incluir diretamente `<chrono>` e `<utility>` em `pricing.cpp`; inspecionado e compilado.
- [x] Validar tamanhos dos vetores de duais antes de acessá-los; inspecionado e compilado.
- [x] Validar valores finitos dos duais; inspecionado e compilado.
- [x] Validar índices das decisões antes de acessar os grupos; inspecionado e compilado.
- [ ] Ampliar verificações de tempo nas demais etapas da heurística.
- [ ] Implementar construção de rotas e controle de tempo durante a busca heurística.
- [x] Montar o PMR com colunas permitidas e acrescentar colunas dos grupos SAME por dia; implementação inspecionada, com verificações específicas dos grupos pendentes.
- [ ] Completar o processamento do nó e garantir viabilidade conforme a Fase 3.1 do roadmap; se optar por artificiais, usar uma fase de viabilidade certificada.
- [ ] Remover o `return true` provisório no início de `columnGeneration()` ao implementar o ciclo.
- [ ] Implementar pricing heurístico seguido de pricing exato quando a heurística não encontrar coluna de custo reduzido negativo.
- [ ] Implementar `columnGeneration()`: resolver o LP, extrair duais, executar pricing e adicionar novas colunas sem duplicação.
- [ ] Repetir o ciclo até o pricing exato provar que não há coluna com custo reduzido negativo; verificar as artificiais e registrar o limite inferior da raiz.
- [ ] Tratar interrupção por tempo sem usar o objetivo de um PMR não convergido como limite inferior válido.

## Depois: soluções inteiras e ramificação

- [x] Implementar `isIntegral()` com tolerância e verificação de valores finitos; inspecionado e compilado.
- [ ] Integrar `isIntegral()` à busca somente após LP ótimo; condicionar fechamento do nó à certificação e montar a solução a partir das variáveis do mestre.
- [x] Implementar `updateIncumbent()` para guardar soluções validadas de menor custo, com tolerância de `1e-6`; atualmente chamado pelo teste local do PMR.
- [x] Implementar `columnAllowed()` respeitando decisões SAME e DIFFERENT (inspeção do código).
- [x] Verificar `columnAllowed()` para SAME, DIFFERENT e múltiplas decisões, além da filtragem e do mapeamento no PMR com decisões.
- [ ] Implementar `selectBranchingPair()` e criar os filhos em `branch()` com decisões de Ryan-Foster.
- [ ] Implementar podas por limitante antes e depois do processamento de cada nó.
- [ ] Adicionar seleção por melhor limitante em `selectNode()`.
- [ ] Implementar `updateGlobalLB()` considerando os nós ainda não resolvidos.

## Etapas posteriores

- [ ] Gravar a solução e o LB no formato oficial em `-o`; a impressão atual no terminal é apenas para inspeção.
- [ ] Implementar a heurística de arredondamento com o pool de colunas e limite `cfg_.heuristic_time`.
- [ ] Preencher estatísticas de colunas, pricing e tempo total, além dos registros de execução.
- [ ] Incrementar os contadores de chamadas heurísticas e exatas quando o pricing for implementado; `logNode()` já imprime esses contadores e os demais dados disponíveis em `stderr`. Integrar os duais ao log por nó após a integração do PMR e validar os registros com a árvore funcional.
- [ ] Definir e implementar o pré-processamento/propagação da EXP2, condicionado a `cfg_.use_preprocessing` (adiado por enquanto).
