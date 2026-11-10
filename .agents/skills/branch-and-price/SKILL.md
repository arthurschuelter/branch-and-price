---
name: branch-and-price
description: Orientar desenvolvimento, revisão e validação do projeto C++ branch-and-price de PLI/UNICAMP, com roteamento por prioridades e conflitos e extensão planejada para bin packing com classes de conflito. Usar ao trabalhar neste repositório, em seu roadmap, TODO, PMR, pricing ou árvore de Ryan–Foster.
---

# Branch-and-Price de PLI

## Contexto e fontes

O checkout de referência está em `/Users/arthurschuelter/Documents/branch-and-price`. Se o usuário indicar outro checkout, trabalhar nele. Os caminhos abaixo são relativos à raiz do projeto.

Ler `roadmap.md` e `TODO.md` antes de definir a próxima etapa. O roadmap descreve os dois problemas e seus requisitos; o TODO registra o progresso do roteamento. Conferir os arquivos afetados antes de afirmar que uma etapa está implementada. Consultar `enunciado-roteamento.pdf` ou `enunciado-bin-packing.pdf` quando a tarefa depender do contrato oficial. O nome antigo `enunciado.pdf` ainda aparece no TODO.

Responder em português. Quando o usuário pedir orientação ou inspeção de uma função, manter esse escopo; quando pedir implementação, executar a mudança solicitada. Os registros em `ai/` ajudam a recuperar decisões, mas não substituem a inspeção do código atual.

## Mapa e estado observado

| Caminho | Papel |
| --- | --- |
| `src/main.cpp` | CLI, configuração, execução e impressão para inspeção |
| `src/core/instance.{hpp,cpp}` | Parser de roteamento, conflitos e `evaluateRoute` |
| `src/core/solver_rmp.{hpp,cpp}` | Mestre LP, valores primais e duais via Gurobi |
| `src/core/branch_and_price.{hpp,cpp}` | Colunas, nós, decisões, pool global e busca |
| `src/core/pricing.{hpp,cpp}` | Pricing heurístico e modelo exato de roteamento |
| `docs/pricing-exato.md` | Modelo, validações e limitações do pricing exato |
| `tests/instance_zero.txt` | Exemplo oficial de roteamento, ótimo completo 59 |
| `CMakeLists.txt`, `Makefile`, `.clangd` | C++17, compilação e integração com o editor |
| `ai/` | Histórico de uso de IA |

Estado inspecionado em 09/11/2026; revalidar ao usar:

- Parser, avaliação de rota, colunas unitárias da raiz, incumbente inicial, seleção DFS e relógio monotônico estão implementados.
- `RMPSolver` monta cobertura e dias, insere colunas contínuas e expõe objetivo, valores e duais. `processNode()` forma grupos SAME, filtra colunas, resolve o PMR e valida o primal, os duais e eventual solução inteira. PMR inviável ou interrompido permanece não resolvido; uma solução inteira validada pode melhorar a incumbente, mas não certifica o nó.
- `rmp_` e `pricer_` são criados no construtor. Existem implementações iniciais de pricing heurístico e exato, ainda sem integração ao ciclo. O exato usa um MIP por dia e fluxo para impedir subciclos; consultar `docs/pricing-exato.md`. Faltam validações variadas, interrupções, tratamento de exceções e revisão das tolerâncias e do limite inconclusivo.
- `processNode()` retorna `Unresolved` após o diagnóstico; `columnGeneration()` não é chamada e tem retorno provisório. Ramificação, podas certificadas, LB global, best-bound e arredondamento ainda não estão implementados. `isIntegral()` existe, mas o fechamento depende de convergência certificada.
- `-o` é lido, mas a saída oficial ainda não é escrita. `B&P: OK` não comprova conclusão do algoritmo.
- Bin packing está planejado. O pacote fornecido `projeto/`, `SolveBinPacking` e `BinPackingData` ainda não estão no checkout inspecionado.
- O TODO registra 36 colunas, 12 restrições e objetivo 78 para o PMR unitário do exemplo. Esse resultado não é o ótimo 59 nem um LB certificado. Não apresentar esse registro como teste executado na sessão atual.

A sequência pendente começa pela verificação dos grupos SAME/DIFFERENT e do PMR filtrado, seguida da validação do pricing e integração do ciclo de geração de colunas com certificação. Usar o TODO atualizado para decidir a continuidade quando o usuário não escolher outra etapa.

## Contratos matemáticos

O C++ implementa árvore, ramificação e geração de colunas. Gurobi resolve LP no mestre; MIP é permitido no pricing exato e no arredondamento. Na execução normal, tentar pricing heurístico e chamar o exato quando a heurística não encontrar coluna melhorante.

### Roteamento

- Clientes têm índices `0..n-1`, excluindo o depósito configurado; dias são `1..n-1`. Prioridade do dia `d` fica no índice `d-1`.
- Uma coluna é uma sequência não vazia de clientes distintos e um dia. `route`, `has`, `day` e `cost` devem concordar; obter custo por `evaluateRoute`.
- Cobertura de cada cliente é igualdade a 1; cada dia admite no máximo uma rota. Variáveis LP são contínuas não negativas. Duais de cobertura `u_i` são livres; duais de dia `lambda_d <= 0`.
- Custo reduzido: `rho(R) + sum(pi(i,d) - u_i, i em R) - lambda_d`. Certificação exige pricing exato para todos os dias, ou outro limite demonstrado.
- Usar a matriz de custos fornecida. Coordenadas servem para visualização; não assumir desigualdade triangular. Pricing exato deve impedir subciclos desconectados.
- Prioridades são não decrescentes. Compactar dias para a saída preservando a ordem e recalculando o objetivo.
- Deduplicar por sequência/dia, preservando o papel da ordem no custo.

### Bin packing

- Itens externos são `1..n`; capacidade vem do arquivo. Classes podem se sobrepor, ser vazias ou unitárias. Ignorar repetições dentro da mesma classe.
- Uma coluna é um conjunto não vazio viável, com custo 1 e cobertura igual a 1. Não há depósito, dias, viagens ou duais de dia. Zerar os custos do roteamento não produz esse objetivo.
- Pricing maximiza `sum(u_i*y_i)` sob capacidade e conflitos. Existe coluna melhorante se o ótimo exceder 1 com tolerância; custo reduzido é `1 - sum(u_i, i em B)`.
- Ao integrar o código fornecido, preservar `SolveBinPacking`, `BinPackingData`, `Conflict`, `UpdateBest`, `CheckSolution` e a heurística probabilística First Fit Decreasing. Usar a heurística como incumbente e fonte de colunas, reservando tempo para B&P.
- Deduplicar por conjunto. Após SAME, operar com grupos inteiros e duais agregados, inclusive se algum item tiver dual negativo.

### Certificação, ramificação e tempo

- O objetivo de PMR incompleto é limite superior para o LP completo; não o usar automaticamente como LB para poda. Falha heurística e timeout de pricing não certificam convergência.
- Em bin packing, LB inicial é `max(ceil(sum(s_i)/C), max_t |S_t|)`, usando classes sem repetições e máximo 0 se não há classes. Farley fornece `z_PMR / alpha`, com `alpha >= max(1, max_B sum(u_i))`, duais ótimos do PMR e colunas admissíveis do nó. Cota superior certificada do pricing serve; valor heurístico de sua maximização não serve. Arredondar LB com tolerância controlada.
- Em roteamento com custos não negativos, zero é LB inicial seguro. Não aplicar cotas de quantidade de recipientes ao objetivo de viagens e prioridades.
- Ryan–Foster escolhe somente pares com fração conjunta estritamente entre 0 e 1, com tolerância. SAME exige ambos ou nenhum; DIFFERENT impede ambos. Propagar transitividade e conflitos entre grupos; grupos acima da capacidade ou com conflito interno são inviáveis.
- Inicializar nós com colunas viáveis de grupos SAME inteiros. Em roteamento, construir e custear a sequência de cada grupo; não substituí-lo por um ponto geométrico. PMR filtrado inviável sozinho não prova inviabilidade do nó. Artificiais, se usadas, exigem fase de viabilidade certificada.
- Em roteamento, LP fracionário pode não oferecer par fracionário por diferenças de ordem/dia. Seguir o fechamento por atribuição de blocos aos dias da Fase 3.1 do roadmap; não ramificar num par integral.
- Filhos herdam LB certificado. Na interrupção, incluir nó corrente e demais subproblemas pendentes no LB global. Árvore esgotada implica `LB = UB`.
- Arredondamento sobre o pool fornece apenas UB e exige validação. Usar relógio monotônico e propagar tempo restante. Roteamento permite até `t+h`; bin packing inclui todas as heurísticas no orçamento `T`.

## Compilação e validação

O executável atual é `build/bp`; `roteamento` e `binpack` são nomes planejados. Configurar com uma instalação Gurobi realmente presente e seu sufixo de biblioteca:

```sh
cmake -S . -B build -DENABLE_GUROBI=ON -DGUROBI_HOME="$GUROBI_HOME" -DGUROBI_VERSION=130 -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bp -f tests/instance_zero.txt -s 0 -C 10 -t 60 -h 5 -o results/instance_zero.txt
```

`130` é o padrão do CMake, não garantia sobre a instalação. Verificar ambiente/cache antes de configurar. `ENABLE_GUROBI` é ON por padrão; o CMake rejeita explicitamente a opção OFF. O Gurobi deve estar instalado e configurado também para `make`. Não copiar caminhos antigos dos registros de IA para esta máquina.

Compilação, execução com licença e correção matemática são verificações distintas. O TODO registra execução anterior fora do sandbox por HostID; solicitar a permissão necessária se essa limitação reaparecer. Não reconfigurar licença para contorná-la.

Para PMR, conferir status antes de ler atributos, correspondência entre valores e colunas, cobertura, uso de dias e objetivo recalculado com tolerância explícita. Para pricing, árvore ou limites, usar enumeração independente em instâncias pequenas e verificar `LB <= OPT <= UB`, incluindo interrupções. Os exemplos completos têm ótimos 59 em roteamento e 4 em bin packing; o fixture do segundo ainda precisa ser criado.

A saída oficial contém UB, LB global certificado, número de rotas/recipientes e uma linha por rota/recipiente, sem contagem de itens no início dessas linhas. Roteamento omite depósito e usa a linha como dia; bin packing mantém IDs `1..n` e chama `CheckSolution`.

Ao atualizar progresso, marcar no TODO apenas o efetivamente implementado e verificado, distinguindo inspeção, compilação e execução. A entrega planejada usa `codigo/`, `testes/`, `ia/` e `README`, embora o checkout atual use outros nomes. O roadmap exige prompts e respostas completos; resumos em `ai/` não substituem esse histórico. Não inventar testes, conversas ou erros da IA para o relatório.
