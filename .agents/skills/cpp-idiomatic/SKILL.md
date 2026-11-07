---
name: cpp-idiomatic
description: Revisar e refatorar o C++17 deste projeto branch-and-price para torná-lo idiomático, claro e seguro, com foco em RAII, ownership, interfaces, biblioteca padrão e tratamento de erros. Usar para avaliar qualidade de código ou aplicar melhorias de C++; para formulação e implementação do algoritmo, consultar também a skill branch-and-price.
---

# Revisão de C++ idiomático

## Escopo e abordagem

Esta skill pertence ao repositório que contém `.agents/skills/cpp-idiomatic`. Trabalhar no checkout atual, sem depender de um caminho absoluto de usuário.

Responder em português. Se o pedido for avaliar ou revisar, apresentar achados e sugestões; se for melhorar ou refatorar, implementar as melhorias no escopo solicitado e verificar o resultado. Priorizar correção, lifetime e clareza antes de preferências cosméticas. Explicar o benefício concreto de cada mudança relevante, preservando o caráter didático do projeto.

Confirmar o padrão em `CMakeLists.txt`: atualmente C++17. Não introduzir `std::span`, ranges, concepts, `std::format`, `std::expected` ou outras facilidades de padrões posteriores sem uma migração solicitada. Usar os recursos disponíveis que simplifiquem o código; concisão, quantidade de templates e novidade não são medidas de qualidade.

Inspecionar o diff existente, os arquivos solicitados e seus chamadores. Consultar `TODO.md` para distinguir funções implementadas de esqueletos. Para alterações que atinjam PMR, pricing, árvore ou soluções, ler as partes relevantes de `roadmap.md` e a [skill do projeto](../branch-and-price/SKILL.md). Refatoração não deve completar automaticamente etapas pendentes do algoritmo.

## Critérios de revisão

### Recursos, ownership e lifetime

- Preferir valores e RAII para expressar propriedade. Usar referências para dependências obrigatórias não proprietárias e ponteiros quando nulidade ou reassociação fizerem parte do contrato.
- Avaliar `unique_ptr` quando existir um único proprietário; manter `shared_ptr` apenas quando a propriedade for realmente compartilhada. Antes de trocar um tipo, inspecionar cópias, chamadores e relações de lifetime.
- Inicializar membros na declaração ou na lista de inicialização; evitar atribuir vetores/deques vazios no corpo do construtor quando a construção padrão já basta.
- Preferir Rule of Zero. Contudo, um `unique_ptr` para tipo incompleto pode exigir destrutor definido no `.cpp` após a definição do tipo. Não remover esse destrutor mecanicamente.
- Usar `std::move` para transferência real, como retirar `unique_ptr` da fronteira. Não usar `return std::move(local)` sem justificativa: pode impedir NRVO. Conferir usos após move e invalidação de referências/iteradores.
- Na integração Gurobi, preservar a vida do ambiente enquanto o modelo existir e não reutilizar handles de variáveis/restrições de um modelo substituído por `reset()`.

### Interfaces e organização

- Aplicar `const` a operações observadoras e referências de leitura; passar tipos pequenos por valor e objetos maiores por referência quando não houver transferência/cópia necessária.
- Usar `explicit`, `override`, `[[nodiscard]]` e `noexcept` quando expressem contratos reais. Não marcar como `noexcept` operações que podem alocar ou lançar exceções do solver.
- Separar cálculo, validação, logging e atualização de estado quando isso tornar funções longas mais compreensíveis. Extrair funções com nomes do domínio e entradas claras; evitar um framework genérico para poucos usos.
- Incluir diretamente os headers necessários, remover includes redundantes e reduzir acoplamento com forward declarations quando possível. Não depender de includes transitivos nem usar `using namespace std` em headers.
- Manter convenções locais de nomes e formatação. Remover `this->` redundante pode melhorar leitura, mas não é correção funcional nem motivo para reformatar todo o repositório.

### Containers, algoritmos e eficiência

- Preferir range-for quando o índice não for necessário. Usar algoritmos da STL quando exprimirem melhor a intenção; manter loops claros quando houver múltiplas invariantes ou diagnóstico específico.
- Evitar cópias acidentais com `const auto&` ao iterar objetos maiores. Usar `auto` quando o tipo for evidente; manter tipos explícitos quando esclareçam unidade, índice ou contrato numérico.
- Escolher o container pela operação necessária e pela estabilidade de referências. Em DFS com inserção por `push_back`, retirar com `back` e `pop_back`.
- Tratar conversões entre IDs `int` e tamanhos de containers com limites explícitos; evitar tanto comparação signed/unsigned acidental quanto substituir todos os IDs por `size_t` mecanicamente.
- Usar `reserve` quando o tamanho esperado for conhecido e relevante. Preferir `push_back` para objetos já construídos; `emplace_back` quando construir no lugar trouxer benefício claro.
- Lembrar que `vector<bool>` usa referências proxy. Avaliar efeitos em referências, APIs e algoritmos antes de mantê-lo ou substituí-lo.
- Não alegar ganho de desempenho sem evidência. Evitar otimizações que alterem ordem de visita, desempate, estabilidade numérica ou complexidade sem analisar o impacto.

### Erros, estado e números

- Capturar exceções por referência constante quando apropriado e preservar o diagnóstico original. Não engolir falhas do Gurobi nem converter estados distintos em sucesso genérico.
- Usar tipos de resultado ou enums quando um bool misturar timeout, inviabilidade e convergência. Essa mudança exige atualizar todos os chamadores.
- Validar pré-condições antes de indexar. Conferir overflow de somas de pesos/custos e conversões numéricas, propondo tipos compatíveis com os limites da entrada.
- Manter explícitos os tratamentos de NaN/infinito e as tolerâncias. Não substituir comparações tolerantes por igualdade exata nem alterar tolerâncias como mera limpeza de estilo.

## Invariantes deste projeto

Preservar em qualquer refatoração:

- Correspondência entre `Column.route`, `has`, `day` e `cost`; a ordem da rota faz parte do custo.
- Mapeamento entre variáveis do PMR e colunas filtradas do pool; seus índices podem diferir.
- Consulta de atributos Gurobi somente nos estados que os tornam disponíveis.
- Distinção entre objetivo do PMR incompleto, UB de solução viável e LB certificado. Melhorias de tipos ou nomes não autorizam novas podas.
- Validação de integralidade e viabilidade antes de atualizar a incumbente.
- Orçamento de tempo monotônico e preservação dos nós pendentes em interrupções.

Se uma melhoria exigir corrigir comportamento, explicar a alteração sem apresentá-la como refatoração neutra. Não deduzir equivalência apenas porque o exemplo oficial continua produzindo o mesmo objetivo.

## Verificação e entrega

Compilar com a configuração C++17/Gurobi realmente disponível. Conferir o cache antes de reutilizar `build/`; ele pode conter caminhos antigos. Os detalhes de build estão na skill do projeto. Usar warnings existentes e, se disponíveis e pertinentes, clang-tidy ou sanitizers com as flags corretas do projeto. Evitar correções automáticas em massa.

Para mudanças de lifetime, containers ou controle de fluxo, testar os caminhos afetados, inclusive exceções ou fronteiras relevantes. Para mudanças puramente cosméticas, revisão do diff e compilação quando necessária bastam. Não ampliar para benchmarks ou testes do algoritmo completo sem necessidade.

Ao revisar, apresentar primeiro bugs e riscos, com arquivo/linha, impacto e correção sugerida; distinguir esses achados de melhorias opcionais de estilo. Ao refatorar, resumir mudanças, motivo e verificações executadas, explicitando limitações. Atualizar o TODO apenas quando o progresso funcional ou a validação registrada tiver realmente mudado.
