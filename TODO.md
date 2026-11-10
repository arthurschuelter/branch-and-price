# TODO

## Estado atual

- Roteamento: parser, avaliação de rotas, PMR, geração de colunas, pricing heurístico e exato, ramificação SAME/DIFFERENT, fechamento por blocos, DFS/Best-Bound, arredondamento e saída oficial implementados. A árvore ainda precisa de validação adicional.
- Exemplo oficial: execução com UB = LB = 59; com tempo zero, UB 83 e LB 0, pois a cota inicial é interrompida. A certificação exata por enumeração está limitada a oito clientes e a soluções representáveis na escala 1024.
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
   - [ ] Definir uma cota inferior demonstrada para o pricing de cada dia e os erros numéricos associados; avaliar alternativas à enumeração limitada a oito clientes.
   - [ ] Implementar a construção de LB com arredondamento seguro e distinguir LB inteiro de prova de convergência do LP.
   - [ ] Integrar a certificação à busca sem usar o PMR incompleto ou `ObjBound` diagnóstico como prova; preservar o LB herdado em interrupção ou inconclusão.
   - [ ] Comparar manualmente limites e convergência com enumeração independente em instâncias pequenas, inclusive decisões de nó e interrupções.
4. [ ] Completar e validar o fluxo da árvore: nós inteiros, filhos, podas e LB global em DFS e Best-Bound, inclusive sob interrupção.
5. [ ] Completar logs, opções de experimentos, verificador, arredondamento e saída; preparar relatório e entrega do roteamento.
6. [ ] Integrar bin packing a partir do código oficial, preservando suas interfaces e heurística; implementar pricing e limite de Farley específicos.

Controle de tempo: exemplo oficial conferido com tempos 60, 0,001 e 0 segundos; LP isolado com 1.600 colunas retornou efetivamente `GRB_TIME_LIMIT`. Instância fracionária de três clientes interrompeu nó filho em DFS e Best-Bound, preservando UB 6 e LB 3 (ótimo 4). Em 120 clientes, tempos externos com orçamento 0,1 s foram 0,107 s; com tempo zero variaram de 0,010 a 0,174 s, incluindo inicialização do processo, leitura e escrita. A solução unitária era viável. O relógio começa antes da leitura; leitura, construção da incumbente, escrita e chamadas indivisíveis do Gurobi podem ultrapassar o prazo antes de devolver o controle. Os tempos medidos não estabelecem limite rígido para outras instâncias.

Fechamento por blocos: a atribuição usa método húngaro com custos inteiros e retorna sem fechar se não houver prova suficiente. Verificação manual com suporte fracionário fornecido por cópia temporária: primal e dual certificados, sem par fracionário, UB inicial 6, atribuição 4 e LB 4. Em outra matriz de três blocos, o custo 14 coincidiu com enumeração das seis atribuições. Pares indisponíveis e decisão SAME incompatível retornaram sem solução. Timeout simulado dentro da atribuição preservou UB 6 e LB 4; LB propositalmente reduzido a 3 manteve nó aberto após encontrar UB 4. Exemplo oficial e árvore pequena executados após compilação: 59/59 e 4/4, respectivamente. O suporte fracionário e os casos de falha foram induzidos em cópias temporárias; não houve teste automatizado adicionado ao projeto.

O roadmap contém os contratos matemáticos, os critérios de validação e os requisitos de entrega. Não adicionar testes automatizados sem pedido explícito.
