# TODO

## Estado atual: ciclo de geração de colunas

O ciclo no processamento do nó resolve o PMR, valida e insere candidatas,
chama a heurística e recorre ao exato. Limite inconclusivo e interrupção mantêm
o nó aberto; exceções Gurobi no ciclo são tratadas como erro. No exemplo oficial,
o PMR chegou a 59 e foi obtida uma solução viável de custo 59; isso, isoladamente,
não certifica o LB. Compilação e execução atuais conferidas: saída UB 59, LB inicial
28 e três rotas; o nó permanece aberto sem certificação do pricing.
O usuário informou que a verificação dos grupos, da preparação e da resolução do
primeiro PMR filtrado com SAME(1,3) funcionou. O usuário informou sucesso na
comparação manual do pricing exato com enumeração, inclusive SAME/DIFFERENT;
não foram informadas as instâncias nem os valores observados. Continuam pendentes
verificações de PMR inviável e interrupção, avaliação adicional do pricing e
revisão numérica. Por isso o ciclo ainda não
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
5. Filtragem, mapeamento e formação de grupos SAME implementados. Decisões DIFFERENT internas são detectadas; grupos são avaliados quanto a capacidade e conflitos e recebem colunas para cada dia, sem duplicar sequência/dia. O usuário informou que a verificação dos casos de PMR 5b funciona; não foram informados detalhes adicionais dos casos executados.
6. Tempo restante implementado; `GRB_TIME_LIMIT` e `GRB_INFEASIBLE` retornam nó não resolvido por inspeção. O usuário informou sucesso na verificação manual do primeiro LP filtrado com SAME(1,3); timeout e PMR inviável não foram confirmados nessa verificação (`PMR 6a–6c`).
7. Implementado: verificar integralidade antes de construir `Solution`; LP fracionário válido retorna `std::nullopt`. O usuário informou sucesso na verificação manual do caso fracionário; os detalhes da instância e dos valores não foram informados. `isIntegral()` também consulta e verifica os valores do mestre; sua integração à busca depende de LP ótimo e certificação (`PMR 7–7a`).
8. Concluída no diagnóstico a validação da solução inteira antes de atualizar a incumbente (`PMR 8–8a`).
9. `NodeResult` declarado, retornado por `processNode()` e tratado em `solve()`; implementação inspecionada e compilada. O resultado de convergência ainda depende do pricing (`PMR 9`).
10. O ciclo `CG 1–6` foi iniciado; faltam validação, certificação e condicionar podas e fechamento a ela (`PMR 10`).

Conferir o PMR unitário do exemplo contra o registro 78 e as 36 colunas, sem
interpretar esse resultado como LB certificado ou ótimo completo 59. Não habilitar
a árvore como concluída enquanto `columnGeneration()` mantiver o nó aberto sem certificação.

### Sequência de trabalho em `processNode()`

1. **PMR 5b:** funcionamento informado pelo usuário para a verificação dos grupos e preparação do PMR filtrado.
2. **PMR 6a–6c:** primeiro LP filtrado com SAME(1,3) verificado manualmente conforme informado pelo usuário. Falta conferir em execução timeout e PMR inviável; a inviabilidade do PMR filtrado não certifica inviabilidade do nó.
3. **PMR 7–8:** validação primal/dual ocorre antes da construção condicional de solução inteira; o usuário informou que a verificação manual do comportamento fracionário deu certo. As duas partes ainda estão na mesma função.
4. **PMR 9:** `solve()` trata os resultados atuais de `processNode()`; falta integrar a convergência certificada ao pricing e verificar os casos em execução.
5. **PMR 10:** condicionar podas e fechamento à certificação. Validar e completar o ciclo de geração de colunas e o pricing exato (`CG 1–6`).

- [x] Diagnóstico primal/dual do PMR, verificação de integralidade e retorno de LP fracionário implementados.
- [x] Verificar em execução o retorno para LP fracionário após a alteração: sucesso informado pelo usuário, sem detalhes da instância ou dos valores.
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
- [x] Comparação manual do pricing exato com enumeração independente, inclusive decisões SAME/DIFFERENT: sucesso informado pelo usuário; instâncias e valores não detalhados.
- [ ] Ampliar validação do pricing exato com instâncias pequenas variadas e interrupção durante a otimização; integrar certificação ao ciclo.
- [x] Consulta de `ObjBound` implementada e explicação do pricing exato registrada em `docs/pricing-exato.md`.
- [x] Limites por dia retornados pelo pricing exato e candidata de LB ajustado calculada apenas para diagnóstico ao concluir sem colunas; inspeção, compilação e execução normal do exemplo conferidas novamente. A saída imprime 59 com precisão padrão (arredondada), incumbente 59 e nó aberto. Interrupção, limites indisponíveis e regra numérica ainda não foram verificados em execução; nenhum LB é atualizado.
- [x] Cálculo de diagnóstico extraído para `diagnosticLowerBound()`, com resultado opcional e verificações de estado, dimensões e valores finitos; impressão com precisão `max_digits10`. Compilado e executado no exemplo: candidata `58.999997000000008`, incumbente 59 e nó aberto. Caminhos indisponíveis não exercitados em execução.
- [x] Fluxo do diagnóstico revisto por inspeção: cálculo ocorre somente após pricing exato concluído sem colunas, não atualiza `node.lb` nem fecha o nó; interrupção e dados ausentes continuam sem verificação em execução.
- [x] Distinguir `Inconclusive` de `Error` na interface do pricing exato; implementação inspecionada.
- [ ] Verificar o limite inconclusivo em execução e revisar as tolerâncias antes de certificar.
- [ ] Completar o tratamento de exceções do Gurobi na construção dos solvers e preservar diagnósticos dos status inesperados. Revisar tolerâncias numéricas.
- [x] Tratar exceções Gurobi e padrão na montagem do PMR em `processNode()`: registrar o erro, descartar o modelo e retornar `Error`. Fluxo revisto por inspeção; compilação e execução normal do exemplo conferidas anteriormente. Caminhos de exceção não exercitados em execução.
- [x] Métodos iniciais do `Pricer` integrados ao ciclo de geração de colunas; validação adicional pendente.
- [x] `pricer_` criado e pricing heurístico inicial implementado: combina grupos SAME, verifica decisões e custos, retorna candidatas e confere tempo em partes da busca.
- [ ] Verificar em execução as candidatas da heurística, inclusive decisões, custos reduzidos e interrupção; ampliar controle de tempo nas demais etapas.
- [x] PMR montado com colunas permitidas e grupos SAME por dia; inspeção realizada, com verificações específicas pendentes.
- [ ] Completar o processamento do nó e garantir viabilidade conforme a Fase 3.1 do roadmap; se optar por artificiais, usar uma fase de viabilidade certificada.
- [x] Removido o `return true` provisório de `columnGeneration()`.
- [x] Ciclo tenta pricing heurístico e chama o exato quando a heurística não insere coluna nova; implementação inicial inspecionada e execução anterior do exemplo registrada.
- [x] Ciclo inicial resolve LP, extrai duais, executa pricing e insere colunas novas por sequência/dia no PMR e no pool; execução anterior do exemplo registrada.
- [x] Validar candidatas antes de indexar: rejeitar rota inválida antes de construir `has` e conferir SAME/DIFFERENT com `columnAllowed()` antes de inserir. Inspecionado e compilado; execução do exemplo oficial após a alteração inseriu 82 colunas e chegou a PMR e incumbente 59. Casos inválidos e decisões de nó não foram exercitados nessa execução.
- [x] Inserção reorganizada: deduplicação por sequência/dia centralizada, colunas do pool reutilizadas e PMR descartado em caso de exceção no ciclo. Código inspecionado e compilado; exemplo oficial executado com 82 colunas geradas e incumbente 59. Caminhos de falha não foram exercitados em execução.
- [ ] Revisar custo de `reserve(size + 1)` por coluna nos vetores de índices e variáveis: pode causar realocações repetidas; preservar a preparação de capacidade antes de alterar o modelo.
- [ ] Repetir o ciclo até o pricing exato provar que não há coluna com custo reduzido negativo; verificar as artificiais e registrar o limite inferior da raiz.
- [x] O ciclo mantém o nó não resolvido sem atualizar LB em timeout ou pricing inconclusivo; implementação inspecionada.
- [ ] Verificar em execução os motivos de nó não resolvido, incluindo PMR inviável, timeout e pricing interrompido ou inconclusivo; conclusão sem certificação já foi observada.

`selectBranchingPair()` calcula as frações conjuntas a partir do PMR e do
mapeamento preservado pelo processamento do nó, selecionando par fracionário
mais próximo de 0,5. Implementação inspecionada e compilada; o exemplo da raiz
foi executado, mas não alcança essa função enquanto a certificação está pendente.
O caso sem par mantém o nó aberto; falta implementar o fechamento por blocos.
`branch()` cria os filhos SAME/DIFFERENT com IDs únicos, profundidade incrementada
e LB herdado e desfaz a primeira inserção se a segunda falhar. Código inspecionado e
compilado; execução normal do exemplo não alcança a ramificação enquanto a
certificação está pendente. A criação de filhos e a falha de inserção ainda não
foram verificadas em execução.

### Progresso dos cinco passos anteriores

1. [ ] Implementar a política de certificação do pricing e de LB do nó: fundamentar
   os limites por dia e margens numéricas; só retornar `Converged` e atualizar LB
   quando as condições necessárias forem cumpridas. Interrupção, inconclusão e
   PMR inviável continuam não resolvidos.
2. [ ] Completar o fechamento de LP fracionário sem par Ryan–Foster. `assignBlocks()`
   constrói blocos do suporte e procura uma atribuição viável com colunas do PMR,
   sob o tempo restante; o resultado é apenas incumbente e o nó permanece aberto.
   Falta demonstrar e implementar o fechamento certificado; caminho não executado.
3. [x] Implementar o LB global e o ciclo da fronteira: LB inicial seguro para
   roteamento, mínimo dos nós pendentes incluindo o corrente em interrupções,
   herança pelos filhos e `LB = UB` somente com árvore esgotada.
4. [x] Implementar podas e estratégias de busca: podar somente com inviabilidade
   comprovada ou LB certificado; acrescentar seleção Best-Bound e manter DFS.
A opção Best-Bound foi executada no exemplo com apenas a raiz; a escolha entre
   múltiplos nós e as podas em árvore não foram verificadas em execução.
5. [x] Implementar saída oficial em `-o`: compactar dias, recalcular e validar a
   incumbente, escrever UB, LB global e rotas na ordem exigida pelo enunciado.
   Execução anterior: UB 59, LB 0 e três rotas; com a cota inicial atual, LB 28.

Seleção e criação dos filhos já existem, mas sua execução e o tratamento de
falhas ainda precisam ser conferidos. A ativação da árvore depende da certificação
e do tratamento de todos os resultados de nó; não atribuir LB ao PMR incompleto.
Fundamentar e revisar erro numérico de `ObjBound`, duais e somas antes de integrar
a certificação; manter a candidata fora das podas e do LB do nó. Verificar também interrupção e limite
inconclusivo: não devem certificar convergência nem atualizar o LB. Permanecem
pendentes a conferência de PMR inviável, os caminhos de falha e o tratamento de
exceções na construção dos solvers.

Revisão de formatação: `git diff --cached --check` aponta quatro linhas com
espaços no fim em `branch_and_price.cpp`; não indica falha de compilação.

### Próximos cinco passos de implementação

1. [ ] Implementar certificação do pricing e do LB do nó com uma política de
   erro numérico fundamentada para `ObjBound`, duais e somas; enquanto incompleta,
   manter a candidata de diagnóstico fora de `node.lb` e não retornar `Converged`.
   Avanço: a raiz recebe agora uma cota inteira independente do pricing: soma da
   prioridade mínima por cliente, do arco de saída mais barato de cada cliente e
   da saída mais barata do depósito. Como cada cliente tem uma saída e cada
   solução não vazia tem uma saída do depósito, esses custos são disjuntos.
   A soma satura em 2^52 para evitar arredondamento para cima. Exemplo oficial:
   LB 28, UB 59; com `t=0,h=0`, LB 28 e UB 83. O pricing continua sem certificação.
2. [x] Distinguir as causas de nó não resolvido (PMR inviável, tempo esgotado,
   pricing interrompido/inconclusivo, duplicatas e conclusão sem certificação),
   propagando-as até `solve()` e ao log, preservando nó e LB herdado. Compilado;
   execução normal confirmou o motivo sem certificação. Demais causas não exercitadas.
3. [ ] Completar o fechamento por blocos após convergência certificada: garantir
   atribuição inteira ótima dos blocos aos dias, validar a solução e só fechar o
   nó quando a condição de otimalidade estiver demonstrada; se houver interrupção,
   preservar o nó aberto.
4. [x] Limitar o orçamento do arredondamento ao menor entre `h` a partir do início
   da heurística e o prazo global `t0 + t + h`, com verificações durante construção
   e extração; se a busca parar antes de `t`, a heurística pode iniciar imediatamente.
   Compilado; exemplos com `t=60,h=2` e `t=0,h=1` executados. Tempo em instâncias
   grandes e interrupção durante otimização ainda precisam de verificação.
5. [ ] Integrar o tratamento completo do nó convergido à árvore: solução inteira,
   seleção e criação dos filhos, LB global e podas; verificar os casos com mais
   de um nó e o fechamento por blocos antes de considerá-la funcional.

O MIP de arredondamento usa o pool global, reavalia as rotas selecionadas e
atualiza somente UB. No fechamento por blocos, uma incumbente igual ao LB já
certificado permite fechar o nó; caso contrário, ele continua aberto. A própria
certificação e a execução desse caminho continuam pendentes.

## Depois: soluções inteiras e ramificação

- [x] isIntegral() implementado; integração à busca permanece pendente.
- [ ] Integrar `isIntegral()` à busca somente após LP ótimo; condicionar fechamento do nó à certificação e montar a solução a partir das variáveis do mestre.
- [x] updateIncumbent() e columnAllowed() implementados; filtragem e mapeamento com decisões verificados.
- [x] Implementar `selectBranchingPair()` e preservar mapeamento das colunas; compilado, ainda sem verificação em execução da seleção.
- [x] Revisão por inspeção: a seleção ignora o depósito, soma valores por par, valida os índices e escolhe somente frações em `(eps, 1-eps)`; empate mantém o primeiro par. O nó sem par é preservado enquanto o fechamento por blocos não existe. Execução direta da seleção segue pendente.
- [x] Criar filhos SAME/DIFFERENT em `branch()` com IDs únicos, profundidade incrementada, decisões e LB herdados; inserção dos dois é desfeita se a segunda falhar. Implementação inspecionada e compilada, ainda sem verificação direta em execução.
- [ ] Completar fechamento por blocos quando não existir par fracionário: a atribuição pode atualizar UB e fechar o nó se UB atingir LB certificado; verificar criação e processamento dos filhos após integração da convergência certificada.
- [x] Implementar podas por LB antes e depois do processamento; execução em árvore pendente.
- [x] Adicionar seleção por melhor limitante em `selectNode()` e opções `--dfs`/`--bestbound`; execução em árvore pendente.
- [x] Implementar `updateGlobalLB()` considerando nós pendentes; nó interrompido retorna à fronteira antes do cálculo. A raiz recebe cota inteira inicial demonstrada. Verificação de árvore completa pendente.

## Etapas posteriores

- [ ] Após concluir um fluxo funcional de roteamento, avaliar e extrair uma camada comum de branch-and-price ao implementar bin packing; manter formulação, pricing, colunas e regras específicas por problema. Considerar extensões futuras, como scheduling, sem presumir a mesma regra de branching ou os mesmos limites.
- [x] Gravar solução compactada, validada e LB no formato oficial em `-o`; exemplo oficial conferido manualmente (59, 28, três rotas após a cota inicial).
- [x] Implementar a heurística de arredondamento com MIP binário sobre o pool global, cobrindo cada cliente uma vez e limitando um percurso por dia. Solução extraída é reavaliada e atualiza apenas a incumbente. Compilação e execução manual do exemplo com `-h 2`: MIP encontrou 59, saída UB 59 e LB 0. Com `-t 0 -h 2`, MIP unitário encontrou 78 e saída registrou UB 78 e LB 0. Interrupção durante o MIP e respeito aos limites em instâncias maiores ainda não foram verificados.
- [x] Contadores de colunas geradas e chamadas heurísticas/exatas incrementados no ciclo e exibidos em `logNode()`; execução anterior registrada.
- [ ] Preencher tempo de pricing e demais estatísticas; tempo total já é medido ao final de `solve()`. Organizar os duais no log por nó e validar os registros com a árvore funcional.
- [ ] Definir e implementar o pré-processamento/propagação da EXP2, condicionado a `cfg_.use_preprocessing` (adiado por enquanto).
