# Roadmap do Projeto de PLI: Branch-and-Price
**Problema de Roteamento com Prioridades e Conflitos**  
*Disciplina: Programação Linear Inteira — IC/UNICAMP*  
*Professor: Prof. Flávio K. Miyazawa*

---

## 📌 Visão Geral e Objetivos Pedagógicos

Este roadmap foi desenhado para guiar o desenvolvimento do projeto prático individual e estruturar suas interações com LLMs. 

### Objetivos do Trabalho
1. **Domínio Teórico e Prático de Branch-and-Price:** Formular o Problema Mestre Restrito (PMR), entender dualidade, implementar Geração de Colunas e aplicar ramificação de Ryan-Foster.
2. **Uso Restrito do Gurobi:** O Gurobi deve atuar puramente como solver de Programação Linear (LP) para relaxações dos nós e para o subproblema de pricing inteiros/heurística de arredondamento. A árvore de busca e o gerenciamento de colunas são codificados em C++.
3. **Uso Transparente e Crítico de LLM:** Registrar todo o histórico de interações na pasta `ia/` e identificar falhas conceituais/código gerado incorretamente para o relatório.

---

## 📅 Visão Geral das Fases

```
[Fase 0] Modelagem Matemática & Exemplo Manual
   │
[Fase 1] Infraestrutura, Parser & Verificador
   │
[Fase 2] Geração de Colunas no Nó Raiz (Ponto de Controle)
   │
[Fase 3] Árvore Branch-and-Price (Ryan-Foster & Heurística -h)
   │
[Fase 4] Bateria de Experimentos & Estudos de Ablação
   │
[Fase 5] Relatório, Histórico de IA & Empacotamento
   │
[Fase 6] Preparação para a Entrevista Individual
```

---

## 🔬 Fase 0: Fundamentação Matemática e Exemplo Manual

### 0.1 Formulação Matemática

#### Problema Mestre Restrito (PMR)
Seja $\Omega$ o conjunto de todas as sequências de clientes distintos que não contêm o depósito e respeitam a capacidade $w(R) \le C$ e o grafo de conflitos $G=(V,E)$. A ordem dos clientes faz parte da rota.  
Seja $d \in \{1, \dots, n-1\}$ o dia de execução da rota.

Variável de decisão binária:  
$$x_{R,d} = \begin{cases} 1, & \text{se a rota } R \text{ for executada no dia } d \\ 0, & \text{caso contrário} \end{cases}$$

Parâmetros:
- Para $R=(i_1,\ldots,i_m)$, $\rho(R)=c(s,i_1)+\sum_{k=2}^{m}c(i_{k-1},i_k)+c(i_m,s)$, usando a matriz de custos de deslocamento da instância. A ordem já está especificada em $R$.
- $\pi(R,d) = \sum_{i \in R} \pi(i,d)$: Custo de prioridade da rota $R$ no dia $d$.
- $c_{R,d} = \rho(R) + \pi(R,d)$: Custo total da rota $R$ no dia $d$.

**Formulação Primal do Mestre:**
$$\min \sum_{R \in \Omega} \sum_{d=1}^{n-1} c_{R,d} \cdot x_{R,d}$$

Sujeito a:
1. **Atendimento de cada cliente $i \in V \setminus \{s\}$ em exatamente um dia:**
   $$\sum_{R \in \Omega : i \in R} \sum_{d=1}^{n-1} x_{R,d} = 1, \quad \forall i \in V \setminus \{s\} \quad (\text{variáveis duais } u_i)$$
2. **No máximo uma rota por dia $d$:**
   $$\sum_{R \in \Omega} x_{R,d} \le 1, \quad \forall d \in \{1, \dots, n-1\} \quad (\text{variáveis duais } \lambda_d \le 0)$$
3. **Domínio:**
   $$x_{R,d} \ge 0, \quad \forall R \in \Omega, \forall d \in \{1, \dots, n-1\}$$

#### Custo Reduzido e Subproblema de Pricing
Dado o vetor de preços duais $(u_i)_{i \in V \setminus \{s\}}$ e $(\lambda_d)_{d=1}^{n-1}$, o custo reduzido $\bar{c}_{R,d}$ de uma variável $x_{R,d}$ é dado por:
$$\bar{c}_{R,d} = c_{R,d} - \sum_{i \in R} u_i - \lambda_d = \rho(R) + \sum_{i \in R} \left(\pi(i,d) - u_i\right) - \lambda_d$$

O subproblema de **Pricing** busca encontrar uma rota válida $R^*$ e um dia $d^*$ tais que $\bar{c}_{R^*,d^*} < -\epsilon$ (onde $\epsilon > 0$ é a tolerância numérica, ex: $10^{-6}$).

### 0.2 Exemplo Calculado à Mão (Seção 8 do Enunciado)
- Instância com $s=0$, 6 clientes, $C=10$, distância de Manhattan, $\pi(i,d) = a_i \cdot d$.
- Escolha uma solução fracionária ou uma iteração da relaxação linear.
- **Checklist para o Relatório:**
  - [ ] Registrar os valores numéricos de $u_i$ e $\lambda_d$.
  - [ ] Demonstrar o cálculo detalhado de $\bar{c}_{R,d}$ para pelo menos duas rotas candidatas.
  - [ ] Para um nó com solução fracionária, mostrar a matriz $f_{i,j} = \sum_{R: i,j \in R} \sum_d x_{R,d}$ e escolher o par de Ryan-Foster com $f_{i,j}$ mais próximo de $0.5$.

---

## 🛠️ Fase 1: Infraestrutura de Software, Parser e Ferramentas Auxiliares

### 1.1 Estrutura do Repositório Git
A estrutura deve seguir rigorosamente a especificação do enunciado:

```text
roteamento/
├── codigo/
│   ├── Makefile
│   ├── main.cpp
│   ├── instance.hpp / instance.cpp
│   ├── solver_rmp.hpp / solver_rmp.cpp
│   ├── pricing.hpp / pricing.cpp
│   ├── branch_and_price.hpp / branch_and_price.cpp
│   └── utils.hpp
├── verificador/
│   ├── verificador.cpp
│   └── Makefile
├── testes/
│   ├── enum_exata.cpp (Algoritmo de enumeração completa para n < 8)
│   ├── gerador_instancias.py
│   └── rodar_experimentos.py
├── ia/
│   └── conversas_llm.md (ou PDFs/HTMLs numerados cronologicamente)
├── relatorio.pdf
└── README
```

### 1.2 Parser e CLI
O programa principal em C++ deve aceitar rigorosamente os seguintes parâmetros via linha de comando:
- `-f <arquivo>`: Caminho para a instância de entrada.
- `-s <depósito>`: Índice do vértice que atua como depósito.
- `-C <capacidade>`: Capacidade máxima do veículo (inteiro).
- `-t <segundos>`: Tempo limite de execução para o Branch-and-Price.
- `-h <segundos>`: Tempo adicional opcional para a heurística de arredondamento.
- `-o <arquivo>`: Arquivo de saída com a solução e o limitante inferior.

### 1.3 Formato Oficial da Instância

O arquivo de entrada segue a Seção 10.2 do `enunciado.pdf`; comentários em linhas iniciadas por `#` são ignorados:

1. `n m`: número de vértices e número de arestas de conflito.
2. `n` linhas, uma por vértice `i`: `w_i x_i y_i pi(i,1) ... pi(i,n-1)`. As coordenadas são apenas para visualização; os custos de prioridade são fornecidos para cada dia.
3. `n(n-1)/2` linhas `i j c(i,j)`, uma para cada par `0 <= i < j < n`. Os custos são simétricos; a diagonal é zero.
4. `m` linhas `i j` com as arestas do grafo de conflitos.

O depósito é informado por `-s`, a capacidade por `-C`. `tests/instance_zero.txt` deve permanecer como o exemplo oficial da Seção 8, cujo ótimo é 59.

As coordenadas não determinam `c(i,j)`. A distância de Manhattan e a fórmula $\pi(i,d)=a_i d$ valem para o exemplo da Seção 8; instâncias gerais fornecem a matriz de custos e a tabela de prioridades explicitamente.
No projeto, os custos de prioridade dos clientes devem ser não decrescentes ao longo dos dias (hipótese do enunciado na Seção 11).

### 1.4 Verificador Independente e Enumeração Completa
- [ ] **Verificador (`verificador.cpp`):**
  - Lê o arquivo de entrada e o de saída.
  - Valida partição de clientes, capacidade $w(R) \le C$, ausência de arestas de conflito em cada rota e recálculo exato dos custos $\rho(R)$ e $\pi(R,d)$.
  - Garante que $\text{Limitante Inferior} \le \text{Custo da Solução}$.
- [ ] **Enumeração Completa (`enum_exata.cpp`):**
  - Gera todas as partições válidas de $V \setminus \{s\}$ e todas as permutações de visita e ordenações de dias para $n < 8$.
  - Serve como a "pólice de seguro" para validar se o Branch-and-Price está encontrando a solução ótima global em instâncias pequenas.

---

## 🚀 Fase 2: Geração de Colunas no Nó Raiz (Ponto de Controle)

*Esta fase corresponde ao primeiro entregável formal do trabalho.*

### 2.1 Colunas Iniciais
Para garantir que o PMR seja viável desde a iteração 0 no nó raiz:
- Criar colunas unitárias: para cada cliente $i \in V \setminus \{s\}$ e cada dia $d \in \{1, \dots, n-1\}$, criar a rota $R = (i)$ com custo $c_{R,d} = c(s,i) + c(i,s) + \pi(i,d)$.

### 2.2 Loop de Geração de Colunas

```
               ┌────────────────────────┐
               │ Resolver PMR no Gurobi │
               └───────────┬────────────┘
                           │ Extrair duais u_i e λ_d
                           ▼
               ┌────────────────────────┐
               │   Pricing Heurístico   │
               └───────────┬────────────┘
                           │ Encontrou c̄ < -ε ?
                     ┌─────┴─────┐
                 Sim │           │ Não
                     ▼           ▼
          ┌──────────────┐  ┌──────────────────────┐
          │ Adicionar    │  │    Pricing Exato     │
          │ colunas ao   │  └──────────┬───────────┘
          │ PMR          │             │ Encontrou c̄ < -ε ?
          └──────┬───────┘       ┌─────┴─────┐
                 │           Sim │           │ Não
                 │               ▼           ▼
                 │        ┌──────────────┐  ┌─────────────┐
                 │        │ Adicionar    │  │ Convergiu!  │
                 │        │ colunas ao   │  │ Limitante   │
                 │        │ PMR          │  │ Raiz Obtido │
                 └───────>└──────┬───────┘  └─────────────┘
                                 │
                                 └──────> (Repetir Loop)
```

### 2.3 Estratégias de Pricing
1. **Pricing Heurístico:**
   - Heurística Gulosa / Busca Local / Randomized Set Packing para construir rotas $R$ válidas maximizando $\sum_{i \in R} (u_i - \pi(i,d)) - \rho(R)$.
   - Rápida execução para evitar chamadas custosas ao solver exato.
2. **Pricing Exato:**
   - Formulável como um MIP (ou enumeração explícita para instâncias de tamanho moderado) para cada dia $d \in \{1, \dots, n-1\}$.
   - Deve respeitar restrições de capacidade, fluxo do TSP e restrições do grafo de conflitos $G=(V,E)$.

---

## 🌳 Fase 3: Árvore de Branch-and-Price e Ramificação de Ryan-Foster

### 3.1 Ramificação de Ryan-Foster
Se a solução do PMR for fracionária:
1. Calcular para cada par de clientes $i, j \in V \setminus \{s\}$ com $i < j$:
   $$f_{i,j} = \sum_{R \in \Omega : i,j \in R} \sum_{d=1}^{n-1} x_{R,d}$$
2. Selecionar o par $(i^*, j^*)$ cujo valor $f_{i^*, j^*}$ esteja mais próximo de $0.5$.
3. Criar dois ramos na árvore de busca:
   - **Ramo SAME ($x_{i^*,j^*} = 1$):** Os clientes $i^*$ e $j^*$ **devem** ser atendidos na mesma rota.
     - *No PMR:* Eliminar colunas existentes onde exatamente um dos dois está presente.
     - *No Pricing:* Tratar $i^*$ e $j^*$ como um único super-cliente ou impor restrição no MIP de pricing.
   - **Ramo DIFFERENT ($x_{i^*,j^*} = 0$):** Os clientes $i^*$ e $j^*$ **não podem** ser atendidos na mesma rota.
     - *No PMR:* Eliminar colunas existentes onde ambos $i^*$ e $j^*$ estão juntos.
     - *No Pricing:* Adicionar a aresta $\{i^*, j^*\}$ ao grafo de conflitos temporário do nó.

### 3.2 Estratégias de Busca e Gestão da Árvore
- Implementar busca em profundidade (**DFS**) e busca pelo melhor limitante (**Best-Bound**).
- Tratar a viabilidade do PMR em nós profundos (se a poda por branching eliminar colunas suficientes a ponto de deixar o PMR inviável, usar variáveis artificiais com custo Big-$M$).

### 3.3 Heurística de Arredondamento (`-h`)
- Quando o tempo limite `-t` for atingido, pegar todas as colunas geradas até o momento no nó atual ou na árvore global.
- Resolver o PMR impondo integridade nas variáveis $x_{R,d} \in \{0,1\}$ usando o Gurobi MIP por até `-h` segundos para tentar atualizar a solução incumbente (Upper Bound).

---

## 📊 Fase 4: Bateria de Experimentos e Estudos de Ablação

O relatório exige uma avaliação experimental com tabelas, gráficos e análises detalhadas.

### 4.1 Conjunto de Experimentos (Ablações Obrigatórias)

| ID | Estudo de Ablação | Configuração A | Configuração B |
| :--- | :--- | :--- | :--- |
| **EXP1** | Estratégia de Pricing | Apenas Pricing Exato | Pricing Heurístico + Pricing Exato |
| **EXP2** | Pré-processamento | Sem Pré-processamento | Com Pré-processamento / Propagação |
| **EXP3** | Regra de Ryan-Foster | Par mais próximo de $0.5$ | Regra Alternativa (ex: maior custo reduzido acumulado) |
| **EXP4** | Busca na Árvore | Busca em Profundidade (DFS) | Melhor Limitante (Best-Bound) |
| **EXP5** | Impacto do `-h` | Sem Heurística de Arredondamento | Com Heurística de Arredondamento |

### 4.2 Automação dos Testes
- Criar script em Python (`testes/rodar_experimentos.py`) para executar todas as instâncias e gerar relatórios em CSV/JSON contendo:
  - Tempo total e tempo gasto no pricing.
  - Número de nós explorados.
  - Número total de colunas geradas.
  - Gap de integridade final ($\frac{\text{UB} - \text{LB}}{\text{UB}} \times 100\%$).

---

## 📝 Fase 5: Documentação, Relatório e Histórico de IA

### 5.1 Organização do Diretório `ia/`
- Salvar todos os logs de conversas com ChatGPT, Claude, Gemini ou outras LLMs.
- Garantir a ordem cronológica e a inclusão de **Prompts e Respostas Íntegras**.

### 5.2 Estrutura Obrigatória do `relatorio.pdf`
1. **Descrição do Algoritmo:** Formulando o Mestre, Pricing, Branching, Estruturas de Dados e RMP Feasibility.
2. **Iteração Calculada à Mão:** Exemplo numérico da Seção 8 conferido com a saída do log do programa (`stderr`).
3. **Experimentos e Discussão:** Tabelas e gráficos comparativos dos experimentos EXP1 a EXP5.
4. **Onde a IA Errou:** Pelo menos **dois casos concretos** onde a LLM gerou código/conceito incorreto ou ineficiente e como você identificou e corrigiu a falha.
5. **Limitações do Algoritmo:** Análise de gargalos e tipos de instâncias difíceis.
6. **Resultados da Heurística de Arredondamento (`-h`).**

---

## 🎤 Fase 6: Preparação para a Entrevista Individual

A avaliação final conta com uma entrevista individual ao vivo. O checklist de domínio teórico/prático inclui:

- [ ] **Dualidade e Custo Reduzido:** Explicar como a combinação de $u_i$ e $\lambda_d$ direciona a geração de colunas e por que o limitante inferior só é válido quando o pricing exato provar ausência de custos reduzidos negativos.
- [ ] **Modificação do Pricing sob Ryan-Foster:** Demonstrar como as restrições de SAME e DIFFERENT alteram a estrutura do subproblema de pricing e o grafo de conflitos.
- [ ] **Controle do Solver:** Explicar por que o Gurobi não faz o B&P sozinho e como sua estrutura C++ gerencia o ciclo de vida dos nós.
- [ ] **Alteração de Código ao Vivo:** Estar preparado para realizar pequenas alterações e recompilar o projeto na frente do professor.

---

## 💬 Guia de Prompts para Interagir com LLMs

Abaixo estão modelos de prompts estratégicos estruturados para você utilizar durante o desenvolvimento. Eles foram formulados para atuar como um **tutor/coach de programação e PLI**, auxiliando na resolução de problemas sem substituir o seu aprendizado.

### Prompt 1: Validação da Formulação e Dualidade (Fase 0)
```text
Atue como um professor especialista em Programação Linear Inteira e Branch-and-Price.
Estou modelando o Problema de Roteamento com Prioridades e Conflitos.
Minhas variáveis do problema mestre são x_{R,d} (binárias, indicando se a rota válida R roda no dia d).

Aqui estão as minhas restrições:
1. Cada cliente i é atendido exatamente uma vez.
2. No máximo uma rota é realizada por dia d.

Por favor:
1. Escreva o Problema Mestre Restrito (PMR) na forma primal.
2. Derive formalmente as variáveis duais associadas a cada restrição.
3. Demonstre passo a passo como obter a equação do custo reduzido c̄_{R,d}.
4. Explique como o subproblema de pricing utiliza esses valores duais.

Não escreva código ainda, foque exclusivamente na explicação matemática rigorosa.
```

### Prompt 2: Modelagem do Pricing e Modificação sob Ryan-Foster (Fase 2 e 3)
```text
Estou implementando o subproblema de pricing para um algoritmo de Branch-and-Price em C++ com Gurobi.
O objetivo do pricing é encontrar uma rota R e um dia d que minimizem o custo reduzido.

Preciso entender exatamente como aplicar o Branching de Ryan-Foster no subproblema de pricing:
1. Quando temos uma decisão SAME(i, j) (os clientes i e j DEVEM estar na mesma rota), como isso deve ser imposing no modelo de pricing do Gurobi? E em uma heurística de busca local?
2. Quando temos uma decisão DIFFERENT(i, j) (os clientes i e j NÃO Podem estar na mesma rota), como isso altera o grafo de conflitos original?

Forneça explicações conceituais e um trecho de exemplo em C++ utilizando a API C++ do Gurobi.
```

### Prompt 3: Diagnóstico de Erros Numéricos e Viabilidade no PMR (Fase 3)
```text
Estou enfrentando um problema de inviabilidade (INFEASIBLE) no meu Problema Mestre Restrito (PMR) em nós mais profundos da árvore de Branch-and-Price.
Isso ocorre quando as restrições de Ryan-Foster filtram muitas colunas existentes e o solver não encontra rotas válidas para cobrir todos os clientes.

1. Qual é a prática padrão em Branch-and-Price para garantir a viabilidade do PMR em todos os nós da árvore?
2. Como posso implementar variáveis artificiais com penalização Big-M para evitar inviabilidade no Gurobi?
3. Como essas variáveis artificiais afetam a extração dos valores duais?

Explique a teoria e mostre um pseudocódigo do ajuste no PMR.
```

### Prompt 4: Identificação de Erros da IA para o Relatório (Fase 5)
```text
Analise o seguinte trecho de código/conceito que você (ou outra LLM) gerou anteriormente para o cálculo do custo reduzido e gerenciamento de colunas duplicadas:

[INSERIR CÓDIGO OU RESPOSTA DA IA AQUI]

Por favor, identifique:
1. Há algum erro de lógica ou erro numérico na fórmula do custo reduzido?
2. Como esse código trata colunas duplicadas no PMR?
3. Qual seria a forma correta e eficiente de reescrever este trecho em C++ moderno?

Essa análise será utilizada para documentar a seção "Onde a IA errou" do meu relatório final.
```

---

## 🎯 Resumo do Checklist de Entrega

- [ ] Código C++ limpo com `Makefile` configurável em `codigo/`
- [ ] Executável compila e roda com a interface de linha de comando solicitada
- [ ] Verificador em `verificador/` validando arquivos de saída
- [ ] Algoritmo de enumeração completa em `testes/`
- [ ] Diretório `ia/` contendo todas as conversas íntegras com LLMs
- [ ] Histórico de commits no repositório Git
- [ ] `relatorio.pdf` completo com todas as 6 seções obrigatórias
- [ ] Domínio total do código para a entrevista individual
