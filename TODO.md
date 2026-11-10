# TODO

## Estado atual

- Roteamento: parser, avaliação de rotas, PMR, geração de colunas, pricing heurístico e exato, ramificação SAME/DIFFERENT, DFS/Best-Bound, arredondamento e saída oficial implementados. A árvore e o fechamento por blocos ainda precisam de conclusão e validação adicional.
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
2. [ ] Completar o fechamento por blocos após convergência certificada:
   - [ ] Revisar a formação de blocos a partir do suporte primal certificado e a escolha da melhor sequência disponível por bloco/dia.
   - [ ] Garantir atribuição ótima aos dias e fechamento somente com prova de otimalidade; manter o nó aberto se a busca for interrompida ou a prova falhar.
   - [ ] Verificar manualmente um LP fracionário sem par Ryan–Foster fracionário, incluindo interrupção e comparação com enumeração independente.
3. [ ] Ampliar a certificação do LP e LB para instâncias gerais com garantia matemática e numérica; comparar com enumeração independente antes de permitir podas.
4. [ ] Completar e validar o fluxo da árvore: nós inteiros, filhos, podas e LB global em DFS e Best-Bound, inclusive sob interrupção.
5. [ ] Completar logs, opções de experimentos, verificador, arredondamento e saída; preparar relatório e entrega do roteamento.
6. [ ] Integrar bin packing a partir do código oficial, preservando suas interfaces e heurística; implementar pricing e limite de Farley específicos.

Controle de tempo: exemplo oficial conferido com tempos 60, 0,001 e 0 segundos; LP isolado com 1.600 colunas retornou efetivamente `GRB_TIME_LIMIT`. Instância fracionária de três clientes interrompeu nó filho em DFS e Best-Bound, preservando UB 6 e LB 3 (ótimo 4). Em 120 clientes, tempos externos com orçamento 0,1 s foram 0,107 s; com tempo zero variaram de 0,010 a 0,174 s, incluindo inicialização do processo, leitura e escrita. A solução unitária era viável. O relógio começa antes da leitura; leitura, construção da incumbente, escrita e chamadas indivisíveis do Gurobi podem ultrapassar o prazo antes de devolver o controle. Os tempos medidos não estabelecem limite rígido para outras instâncias.

O roadmap contém os contratos matemáticos, os critérios de validação e os requisitos de entrega. Não adicionar testes automatizados sem pedido explícito.
