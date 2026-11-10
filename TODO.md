# TODO

## Estado atual: ciclo de geração de colunas

O ciclo no processamento do nó resolve o PMR, valida e insere candidatas,
chama a heurística e recorre ao exato. Limite inconclusivo e interrupção mantêm
o nó aberto; exceções Gurobi no ciclo são tratadas como erro. No exemplo oficial,
o PMR chegou a 59 e foi obtida uma solução viável de custo 59; isso não valida
independentemente o pricing nem certifica o LB. Execução anterior conferida; compilação do código atual conferida, com avisos.
Continuam pendentes os testes dos grupos e do PMR filtrado, enumeração independente
do pricing, testes de interrupção e revisão numérica. Por isso o ciclo ainda não
atribui LB certificado nem fecha o nó após o pricing terminar.

## Instância, parser e avaliação de rota

- [x] Parser de roteamento e avaliação de rotas implementados; exemplo oficial conferido.

## Inicialização do nó raiz (`initRoot`)

- [x] Inicialização da raiz e colunas unitárias implementadas; exemplo oficial com 36 colunas conferido.

## Controle da busca (parcial)

- [x] Seleção DFS e controle de tempo com relógio monotônico implementados.

## PMR inicial da raiz — progresso de 07/11/2026

Implementação e validação inicial da Fase 2.1 do `roadmap.md`, usando apenas as colunas unitárias disponíveis.

- [x] PMR inicial implementado e validado no exemplo oficial: 36 colunas, 12 restrições e objetivo 78. Impressão para diagnóstico e resumo da sessão registrados.

O teste com Gurobi precisou ser executado fora do sandbox para reconhecer o HostID da licença. A compilação passou com avisos em trechos ainda incompletos. O resultado 78 corresponde ao PMR com rotas unitárias; não é o ótimo 59 do problema completo nem um LB certificado. A execução observada selecionou os clientes 1, 3, 5, 6, 4 e 2 nos dias 1 a 6, respectivamente.

## Próxima sessão: concluir inspeção e integrar o PMR

O esqueleto comentado está em `src/core/branch_and_price.cpp`. Os passos 1, 2,
3 e 4 foram realizados; os passos 7 e 8 existem no diagnóstico da raiz. A
integração completa à árvore e a certificação pelo pricing continuam pendentes.

1–4. Concluídos para o diagnóstico da raiz: inspeção dos duais, criação de `rmp_`, incumbente unitária e transferência do PMR para `processNode()`; o nó permanece aberto sem certificação.
5. Filtragem, mapeamento e formação de grupos SAME implementados. Decisões DIFFERENT internas são detectadas; grupos são avaliados quanto a capacidade e conflitos e recebem colunas para cada dia, sem duplicar sequência/dia. O usuário verificou a filtragem e o mapeamento, incluindo SAME, DIFFERENT e múltiplas decisões, e a formação transitiva do grupo `{1, 3, 5}`. Falta verificar os demais casos dos grupos e a viabilidade do PMR filtrado (`PMR 5b`).
6. Tempo restante implementado; `GRB_TIME_LIMIT` e `GRB_INFEASIBLE` retornam nó não resolvido. Falta verificar o LP com colunas filtradas (`PMR 6a–6c`).
7. Implementado: verificar integralidade antes de construir `Solution`; LP fracionário válido retorna `std::nullopt`. `isIntegral()` também consulta e verifica os valores do mestre. Código inspecionado e compilado; falta verificar em execução o caso fracionário e integrar a função à busca após LP ótimo e certificação (`PMR 7–7a`).
8. Concluída no diagnóstico a validação da solução inteira antes de atualizar a incumbente (`PMR 8–8a`).
9. `NodeResult` declarado, retornado por `processNode()` e tratado em `solve()`; implementação inspecionada e compilada. O resultado de convergência ainda depende do pricing (`PMR 9`).
10. O ciclo `CG 1–6` foi iniciado; faltam validação, certificação e condicionar podas e fechamento a ela (`PMR 10`).

Conferir o PMR unitário do exemplo contra o registro 78 e as 36 colunas, sem
interpretar esse resultado como LB certificado ou ótimo completo 59. Não habilitar
a árvore como concluída enquanto `columnGeneration()` mantiver o nó aberto sem certificação.

### Sequência de trabalho em `processNode()`

1. **PMR 5b:** verificar DIFFERENT interno, capacidade, conflitos, criação e deduplicação das colunas dos grupos e viabilidade do PMR filtrado.
2. **PMR 6a–6c:** verificar o LP com colunas filtradas. Timeout e PMR inviável preservam o nó como não resolvido; inviabilidade do PMR filtrado não certifica inviabilidade do nó.
3. **PMR 7–8:** validação primal/dual ocorre antes da construção condicional de solução inteira; o código aceita LP fracionário válido e atualiza a incumbente apenas para solução inteira validada. Verificar o comportamento fracionário após a alteração; as duas partes ainda estão na mesma função.
4. **PMR 9:** `solve()` trata os resultados atuais de `processNode()`; falta integrar a convergência certificada ao pricing e verificar os casos em execução.
5. **PMR 10:** condicionar podas e fechamento à certificação. Validar e completar o ciclo de geração de colunas e o pricing exato (`CG 1–6`).

- [x] Diagnóstico primal/dual do PMR, verificação de integralidade e retorno de LP fracionário implementados.
- [ ] Verificar em execução o retorno para LP fracionário após a alteração.
- [x] Solução inteira validada antes de atualizar incumbente; estados de nó não resolvido, inviável e erro tratados provisoriamente.
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

- [x] Custo reduzido, interface do pricing e modelo inicial exato implementados; restrições de rota, capacidade, conflitos, decisões e fluxo compiladas.
- [ ] Validar em execução a conexão ao depósito e a eliminação de subciclos.
- [x] Objetivo por dia, validação de duais e reconstrução de rotas implementados; comparação temporária com enumeração no exemplo e consulta de ObjBound registradas em docs/pricing-exato.md.
- [ ] Ampliar validação do pricing exato com instâncias pequenas variadas e interrupção durante a otimização; integrar certificação ao ciclo.
- [x] Consulta de `ObjBound` implementada e explicação do pricing exato registrada em `docs/pricing-exato.md`.
- [x] Distinguir `Inconclusive` de `Error` na interface do pricing exato; implementação inspecionada.
- [ ] Verificar o limite inconclusivo em execução e revisar as tolerâncias antes de certificar.
- [ ] Completar o tratamento de exceções do Gurobi: o `catch` atual cobre o ciclo, mas não a montagem do PMR em `processNode()` nem a construção dos solvers; preservar diagnósticos dos status inesperados. Revisar tolerâncias numéricas.
- [x] Métodos iniciais do `Pricer` integrados ao ciclo de geração de colunas; validação adicional pendente.
- [x] `pricer_` criado e pricing heurístico inicial implementado: combina grupos SAME, verifica decisões e custos, retorna candidatas e confere tempo em partes da busca.
- [ ] Verificar em execução as candidatas da heurística, inclusive decisões, custos reduzidos e interrupção; ampliar controle de tempo nas demais etapas.
- [x] PMR montado com colunas permitidas e grupos SAME por dia; inspeção realizada, com verificações específicas pendentes.
- [ ] Completar o processamento do nó e garantir viabilidade conforme a Fase 3.1 do roadmap; se optar por artificiais, usar uma fase de viabilidade certificada.
- [x] Removido o `return true` provisório de `columnGeneration()`.
- [x] Ciclo tenta pricing heurístico e chama o exato quando a heurística não insere coluna nova; implementação inicial inspecionada e execução anterior do exemplo registrada.
- [x] Ciclo inicial resolve LP, extrai duais, executa pricing e insere colunas novas por sequência/dia no PMR e no pool; execução anterior do exemplo registrada.
- [ ] Validar candidatas com segurança antes de indexar: rejeitar rota inválida e índices fora do intervalo antes de construir `has`; conferir explicitamente SAME/DIFFERENT com `columnAllowed()` antes de inserir.
- [ ] Reorganizar a inserção para preservar a correspondência entre pool, índices e PMR em caso de erro; centralizar a identificação de duplicatas por sequência/dia.
- [ ] Repetir o ciclo até o pricing exato provar que não há coluna com custo reduzido negativo; verificar as artificiais e registrar o limite inferior da raiz.
- [x] O ciclo mantém o nó não resolvido sem atualizar LB em timeout ou pricing inconclusivo; implementação inspecionada.
- [ ] Verificar interrupções em execução e distinguir as causas de `Unresolved`, incluindo PMR inviável, timeout e pricing exato concluído sem candidata; não confundir conclusão do pricing com certificação.

Próximos passos: verificar grupos SAME/DIFFERENT e PMR filtrado; corrigir a
validação de candidatas; ampliar a validação independente do pricing e revisar
tolerâncias e exceções antes de integrar a certificação.

## Depois: soluções inteiras e ramificação

- [x] isIntegral() implementado; integração à busca permanece pendente.
- [ ] Integrar `isIntegral()` à busca somente após LP ótimo; condicionar fechamento do nó à certificação e montar a solução a partir das variáveis do mestre.
- [x] updateIncumbent() e columnAllowed() implementados; filtragem e mapeamento com decisões verificados.
- [ ] Implementar `selectBranchingPair()` e criar os filhos em `branch()` com decisões de Ryan-Foster.
- [ ] Implementar podas por limitante antes e depois do processamento de cada nó.
- [ ] Adicionar seleção por melhor limitante em `selectNode()`.
- [ ] Implementar `updateGlobalLB()` considerando os nós ainda não resolvidos.

## Etapas posteriores

- [ ] Após concluir um fluxo funcional de roteamento, avaliar e extrair uma camada comum de branch-and-price ao implementar bin packing; manter formulação, pricing, colunas e regras específicas por problema. Considerar extensões futuras, como scheduling, sem presumir a mesma regra de branching ou os mesmos limites.
- [ ] Gravar a solução e o LB no formato oficial em `-o`; a impressão atual no terminal é apenas para inspeção.
- [ ] Implementar a heurística de arredondamento com o pool de colunas e limite `cfg_.heuristic_time`.
- [x] Contadores de colunas geradas e chamadas heurísticas/exatas incrementados no ciclo e exibidos em `logNode()`; execução anterior registrada.
- [ ] Preencher tempo de pricing, tempo total e demais estatísticas; organizar os duais no log por nó e validar os registros com a árvore funcional.
- [ ] Definir e implementar o pré-processamento/propagação da EXP2, condicionado a `cfg_.use_preprocessing` (adiado por enquanto).
