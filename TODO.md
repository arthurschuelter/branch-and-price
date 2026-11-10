# TODO

## Estado atual

- Roteamento: parser, avaliação de rotas, PMR, geração de colunas, pricing heurístico e exato, ramificação SAME/DIFFERENT, fechamento por blocos, DFS/Best-Bound, arredondamento e saída oficial implementados. A árvore ainda precisa de validação adicional.
- Exemplo oficial: execução com UB = LB = 59; com tempo zero, UB 83 e LB 0, pois a cota inicial é interrompida. A certificação de rotas usa programação dinâmica até 16 clientes e busca exaustiva interrompível acima disso; uma relaxação também fornece limites. A reconstrução racional usa escalas finitas e pode ser inconclusiva.
- O limite calculado a partir de `ObjBound` é apenas diagnóstico. O PMR incompleto, interrupções e pricing inconclusivo não certificam convergência.
- Comparações manuais do pricing exato com enumeração, incluindo conflito, SAME/DIFFERENT e subciclo, estão em `docs/pricing-exato.md`. Houve timeout efetivo em um LP isolado e interrupção em nós filhos; outros estados, como PMR inviável no fluxo, foram exercitados por simulação.
- Bin packing ainda não está implementado; o pacote oficial `projeto/` não está neste checkout.

## Prioridades

1. [x] Implementar controle de tempo, preservando nós, incumbente e LB sob interrupção:
   - [x] Construir incumbente e raiz antes de gerar as demais colunas; criar o ambiente do PMR somente após iniciar o relógio e se houver tempo.
   - [x] Verificar o prazo na preparação do nó e em partes da montagem e extração do pricing exato.
   - [x] Iniciar relógio na entrada do programa, incluindo leitura e criação do ambiente, e preservar solução inicial mesmo após esgotar o prazo.
   - [x] Interromper montagem das restrições do PMR e inserção de colunas de pricing ao expirar; descartar PMR parcial.
   - [x] Revisar extração e validação primal/dual do PMR, pricing e arredondamento, propagando o orçamento às operações com limite de tempo.
   - [x] Medir tempo externo em instância de 120 clientes, incluindo inicialização do processo, leitura, incumbente obrigatória e escrita.
   - [x] Verificar manualmente timeout efetivo no LP, interrupção em nó filho, modelo maior e saída viável; conferir LB global e nós pendentes.
2. [x] Completar o fechamento por blocos após convergência certificada:
   - [x] Validar suporte primal certificado e formar blocos disjuntos que cobrem todos os clientes.
   - [x] Selecionar e revalidar a sequência admissível de menor custo em cada par bloco/dia.
   - [x] Resolver atribuição ótima de blocos a dias distintos, respeitando o prazo e evitando busca fatorial.
   - [x] Atualizar a incumbente validada e fechar o nó somente quando seu custo atingir LB certificado; preservar nó e LB se interrompido ou inconclusivo.
   - [x] Verificar manualmente um LP fracionário sem par Ryan–Foster fracionário, com custos e atribuições conferidos por enumeração independente.
   - [x] Verificar interrupção, pares bloco/dia indisponíveis, decisões de nó e ausência de prova de fechamento; compilar e conferir exemplo oficial.
3. [ ] Ampliar a certificação do LP e LB para instâncias gerais:
   - [x] Construir cota por dia independente do solver a partir de custos mínimos de saída e duais quantizados; demonstrar validade para todas as rotas, inclusive com decisões do nó.
   - [x] Calcular dual viável e LB inteiro com aritmética exata e proteção contra overflow; provar convergência do LP apenas com igualdade primal-dual exata.
   - [x] Integrar a cota geral à busca e preservar a enumeração para reforço em instâncias pequenas; manter LB herdado em interrupção ou inconclusão.
   - [x] Comparar manualmente limites e convergência com enumeração independente em instâncias pequenas e casos maiores, inclusive decisões e interrupções.
   - [x] Ampliar a reconstrução racional de escalas até `2^62`, com checagens de overflow em escala, cobertura, dias, pares da ramificação e acumulação primal; sem igualdade primal-dual exata, não declarar convergência.
   - [ ] Avaliar e tratar casos que ainda excedem `2^62` ou os limites de `int64_t`, mantendo o nó aberto quando não for possível certificar. A busca exata por rotas não tem limite fixo de clientes, mas pode esgotar o tempo.
4. [ ] Completar e validar o fluxo da árvore de acordo com as Fases 3.2 e 4 do roadmap:
   - [ ] Revisar cada resultado de processamento de nó e confirmar que somente inviabilidade provada, LB certificado ou nó convergido autorizam poda/fechamento.
   - [ ] Conferir criação dos filhos SAME/DIFFERENT, herança do LB, seleção DFS e Best-Bound, e atualização do LB global com todos os nós pendentes.
   - [ ] Comparar manualmente UB, LB e nós abertos com enumeração independente em árvores pequenas, para DFS e Best-Bound.
   - [ ] Exercitar interrupções na raiz e em nós filhos, em pricing, solver e fechamento por blocos; confirmar que nó corrente e subproblemas pendentes permanecem representados no LB global.
5. [ ] Completar validação e entrega do roteamento conforme Fases 4 e 5 do roadmap:
   - [ ] Revisar logs por nó e opções de experimentos; garantir os dados exigidos pelo enunciado e execução reprodutível.
   - [ ] Conferir o verificador independente e ampliar a bateria de casos do roadmap: depósito diferente de zero, prioridades constantes/crescentes, custos não métricos, compactação de dias e fracionamento entre colunas com o mesmo conjunto.
   - [ ] Executar enumeração independente em dezenas de instâncias pequenas e comparar `LB <= OPT <= UB`, incluindo os exemplos oficiais já disponíveis.
   - [ ] Preparar ablações, scripts/resultados e relatório de roteamento; conferir os artefatos de entrega exigidos no roadmap.
6. [ ] Integrar bin packing a partir do código oficial, preservando suas interfaces e heurística; implementar pricing e limite de Farley específicos.

Controle de tempo: exemplo oficial conferido com tempos 60, 0,001 e 0 segundos; LP isolado com 1.600 colunas retornou efetivamente `GRB_TIME_LIMIT`. Instância fracionária de três clientes interrompeu nó filho em DFS e Best-Bound, preservando UB 6 e LB 3 (ótimo 4). Em 120 clientes, tempos externos com orçamento 0,1 s foram 0,107 s; com tempo zero variaram de 0,010 a 0,174 s, incluindo inicialização do processo, leitura e escrita. A solução unitária era viável. O relógio começa antes da leitura; leitura, construção da incumbente, escrita e chamadas indivisíveis do Gurobi podem ultrapassar o prazo antes de devolver o controle. Os tempos medidos não estabelecem limite rígido para outras instâncias.

Fechamento por blocos: a atribuição usa método húngaro com custos inteiros e retorna sem fechar se não houver prova suficiente. Verificação manual com suporte fracionário fornecido por cópia temporária: primal e dual certificados, sem par fracionário, UB inicial 6, atribuição 4 e LB 4. Em outra matriz de três blocos, o custo 14 coincidiu com enumeração das seis atribuições. Pares indisponíveis e decisão SAME incompatível retornaram sem solução. Timeout simulado dentro da atribuição preservou UB 6 e LB 4; LB propositalmente reduzido a 3 manteve nó aberto após encontrar UB 4. Exemplo oficial e árvore pequena executados após compilação: 59/59 e 4/4, respectivamente. O suporte fracionário e os casos de falha foram induzidos em cópias temporárias; não houve teste automatizado adicionado ao projeto.

Certificação: cota analítica por arcos de saída; programação dinâmica de rotas até 16 clientes e DFS exaustiva interrompível acima disso; escalas 1024, 720720 e escalas primal/dual independentes reconstruídas por frações contínuas até `2^62`. Oito instâncias pequenas foram comparadas com enumeração independente do ótimo e dos limites de pricing; uma instância de dez clientes certificou LP e LB 11, e um PMR de 18 clientes com valores `1/17` certificou LP e LB 36 na escala adaptativa 17, selecionando também um par fracionário. Uma verificação isolada reconstruiu o denominador 1.000.003; outra certificou um primal em escala 6 e dual em escala 3 sem multiplicar denominadores. Outra instância de 18 clientes e capacidade 1 certificou LP e LB 36 pela DFS, tanto em chamada direta quanto no ciclo completo; com capacidade 18 e tempo curto, a busca interrompida não retornou cota. Para SAME e DIFFERENT, a cota analítica ficou abaixo da cota exata por rotas. O objetivo primal é acumulado por parte inteira e resto, sem multiplicar custo e escala: um caso com arcos `INT_MAX`, escala primal `2^52` e escala dual 1024 passou a certificar o LP. Após a ampliação da representação, a compilação passou; uma verificação isolada certificou escala primal `2^62` com arcos `INT_MAX` e escala dual 1024. O produto racional foi comparado com aritmética inteira arbitrária em 300 casos temporários antes da ampliação e, depois, com 500 casos de divisor até `2^62`. A relaxação pode ser fraca; casos acima dos limites `2^62`/`int64_t` permanecem inconclusivos e não autorizam declarar convergência. Não usar `ObjBound` como certificado.

O roadmap contém os contratos matemáticos, os critérios de validação e os requisitos de entrega. Não adicionar testes automatizados sem pedido explícito.
