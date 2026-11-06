# Roadmap de PLI: Branch-and-Price para Roteamento e Bin Packing

*Disciplina: Programação Linear Inteira — IC/UNICAMP*  
*Professor: Prof. Flávio K. Miyazawa*

## Escopo e fontes

Este plano mantém o suporte ao **Roteamento com Prioridades e Conflitos** e acrescenta o **Bin Packing com Classes de Conflito**. As referências são [enunciado-roteamento.pdf](enunciado-roteamento.pdf) (Projeto 1) e [enunciado-bin-packing.pdf](enunciado-bin-packing.pdf) (Projeto 2). Números de seções abaixo sempre identificam o respectivo documento.

O núcleo reutilizável reúne gestão da árvore, PMR, geração de colunas, decisões SAME/DIFFERENT, limites de tempo e logs. Formulação, pricing, parser, solução e saída são específicos de cada problema. A divisão em módulos é uma decisão de projeto, não uma estrutura de código imposta pelos PDFs.

| Aspecto | Roteamento | Bin packing |
|---|---|---|
| Elementos | Clientes em `0..n-1`, excluindo depósito `s` | Itens em `1..n` |
| Coluna | Sequência de clientes e dia `(R,d)` | Conjunto não vazio `B` |
| Objetivo | Deslocamento + prioridade | Número de recipientes |
| Conflitos | Arestas entre clientes | Classes, possivelmente sobrepostas |
| Pricing | Seleção e ordenação de clientes por dia | Mochila com conflitos |
| Capacidade | Parâmetro `-C` | Campo do arquivo |
| Arredondamento MIP | `-h`, tempo adicional após `-t` | Opcional, dentro de `-t` |

**Não basta zerar deslocamentos e prioridades para obter o novo objetivo:** isso tornaria todas as soluções de roteamento de custo zero. Bin packing exige custo unitário por recipiente e não tem dias nem depósito.

Nos dois projetos, implementar em C++ a árvore, a ramificação e a geração de colunas. Usar Gurobi apenas como solver LP no mestre; MIP é permitido no pricing exato e na heurística de arredondamento. A execução normal tenta pricing heurístico antes do exato, acionando este quando a heurística não encontra coluna de custo reduzido negativo.

## Fase 0 — Formulação e exemplos manuais

### 0.1 Roteamento: mestre e pricing

Seja `J = V \\ {s}`, e seja $\Omega$ o conjunto das sequências não vazias de clientes distintos que respeitam capacidade e conflitos. A ordem de visita faz parte da coluna. Para $d=1,\ldots,n-1$:

$$\rho(R)=c(s,i_1)+\sum_{k=2}^{|R|}c(i_{k-1},i_k)+c(i_{|R|},s),$$
$$c_{R,d}=\rho(R)+\sum_{i\in R}\pi(i,d).$$

O mestre inteiro tem variáveis $x_{R,d}\in\{0,1\}$ e formulação

$$\min\sum_{R\in\Omega}\sum_d c_{R,d}x_{R,d},$$
$$\sum_{R\ni i}\sum_d x_{R,d}=1\quad(i\in J),\qquad
\sum_R x_{R,d}\le1\quad(d=1,\ldots,n-1).$$

No LP, substituir o domínio por $x_{R,d}\ge0$. No PMR, usar apenas as colunas disponíveis. Os duais são $u_i\in\mathbb R$ para cobertura e $\lambda_d\le0$ para dias. O dual completo maximiza $\sum_i u_i+\sum_d\lambda_d$, sujeito a $\sum_{i\in R}u_i+\lambda_d\le c_{R,d}$.

$$\bar c_{R,d}=\rho(R)+\sum_{i\in R}(\pi(i,d)-u_i)-\lambda_d.$$

O pricing procura uma rota/dia com custo reduzido negativo. A certificação deve cobrir todos os dias. Uma formulação MIP deve assegurar seleção, capacidade, conflitos, conexão ao depósito e eliminação de subciclos; restrições de fluxo ou grau isoladas não bastam.

Os custos são simétricos, mas **não se pressupõe desigualdade triangular**. Coordenadas servem apenas para visualização. O projeto exige prioridades não decrescentes no dia (Seção 11.1). Por isso, uma solução inteira com lacunas entre dias pode ser compactada para os dias `1..k`, preservando a ordem e sem aumentar o custo. Recalcular o objetivo após compactar, antes de gravar a saída.

### 0.2 Bin packing: mestre e pricing

Seja $\mathcal B$ o conjunto dos subconjuntos não vazios $B\subseteq I$ com $\sum_{i\in B}s_i\le C$ e $|B\cap S_t|\le1$ para toda classe. O modelo obrigatório (Seção 10.3) é:

$$\min\sum_{B\in\mathcal B}x_B,\qquad
\sum_{B\ni i}x_B=1\quad(i\in I),\qquad x_B\in\{0,1\}.$$

A relaxação usa $x_B\ge0$; o dual maximiza $\sum_i u_i$, sujeito a $\sum_{i\in B}u_i\le1$, com $u_i$ livres. Assim:

$$\bar c_B=1-\sum_{i\in B}u_i,\qquad
\alpha^*=\max_{B\in\mathcal B}\sum_{i\in B}u_i.$$

Uma formulação exata do pricing usa $y_i\in\{0,1\}$, maximiza $\sum_i u_i y_i$ e impõe:

$$\sum_i s_i y_i\le C,\qquad \sum_{i\in S_t}y_i\le1\ (t=1,\ldots,k),\qquad \sum_i y_i\ge1.$$

Há coluna melhorante quando $\alpha^*>1$ (com tolerância numérica na implementação). Não há custos de viagem, variáveis de dia ou duais $\lambda_d$.

### 0.3 Exemplos e conferência manual

- **Roteamento, Seção 8:** depósito 0, seis clientes, capacidade 10; rotas `(1,3)`, `(5,6)`, `(2,4)` nos dias 1, 2, 3, com custo ótimo **59**. Registrar duais $u_i,\lambda_d$, custos reduzidos de pelo menos duas candidatas e um par fracionário para Ryan–Foster; comparar com logs. É requisito do relatório antigo.
- **Bin packing, Seção 7:** tamanhos `(5,5,4,4,3,3,3,3)`, capacidade 10, classes `{1,2}`, `{3,4}`, `{5,6,7}`. Solução `{1,3}`, `{2,5}`, `{4,6,8}`, `{7}`, de valor ótimo **4**. Volume e maior classe dão apenas 3. Em três recipientes, todos teriam carga 10; o item 1, de tamanho 5, não pode acompanhar o item 2, e nenhum subconjunto dos demais soma 5. Conferir duais e pricing também neste caso como teste de desenvolvimento.

## Fase 1 — Infraestrutura, interfaces e verificação

### 1.1 Organização e código fornecido

Manter os requisitos de empacotamento separados dos caminhos atuais do repositório. A entrega usa `codigo/`, `testes/`, `ia/` e `README`; o roteamento exige também `relatorio.pdf` e histórico Git. O verificador de roteamento pertence a `testes/`, não a um diretório obrigatório separado `verificador/`.

Para bin packing, partir do código fornecido em `projeto/` (Seção 10.2): `main.cpp`, `binpacksolver.cpp`, `binpacksolver.h`, `binpacking.h`, `Makefile` e `instancias/`. Preservar a interface `SolveBinPacking` e a integração com `BinPackingData`, inclusive `Conflict`, `UpdateBest` e `CheckSolution`. Implementar o B&P em `binpacksolver.cpp` e arquivos auxiliares. Se o pacote ainda não estiver no repositório, obtê-lo antes dessa integração.

**Manter a heurística probabilística First Fit Decreasing fornecida** como fonte de incumbente e colunas iniciais. Ela combina tamanho, participação em classes e aleatoriedade, em até 1000 rodadas, respeitando o orçamento restante e podendo parar ao atingir o LB. Não a substituir apenas por recipientes unitários. Reservar tempo para o B&P dentro de `T`.

### 1.2 CLI e tempo

Planejar dois executáveis, com interfaces oficiais:

```sh
./roteamento -f inst.txt -s 0 -C 10 -t 60 -h 5 -o saida.txt
./binpack -f inst.txt -o saida.txt -t 60 -r 1
```

- Roteamento: `-f`, `-s`, `-C`, `-t`, `-o`; `-h` opcional. Tempo total até `t+h`, mais gravação.
- Bin packing: `-f`, `-o`, `-t`; `-r` opcional, padrão 1. Capacidade vem da entrada; arredondamento, inicialização e demais etapas contam no tempo real `T`, mais gravação. Não transportar `-h` como tempo adicional para esse modo.
- Usar relógio monotônico e propagar o tempo restante às chamadas LP/MIP, pricing, heurísticas e enumeração. Manter sempre uma incumbente viável e um LB certificado para interrupção.

### 1.3 Entradas oficiais

Ignorar linhas iniciadas por `#` nos dois formatos.

**Roteamento (Seção 10.2):**

1. `n m`.
2. `n` linhas `w_i x_i y_i pi(i,1) ... pi(i,n-1)`.
3. `n(n-1)/2` linhas `i j c(i,j)`, para `0 <= i < j < n`.
4. `m` linhas `i j` de conflitos.

Depósito e capacidade vêm da CLI; peso e prioridades do depósito são ignorados. Conflitos relevantes às rotas são entre clientes. Usar a matriz fornecida, não reconstruí-la pelas coordenadas. Validar capacidade e monotonicidade das prioridades.

**Bin packing (Seção 9.2):**

1. `n C k`.
2. Uma linha com `n` tamanhos inteiros positivos, todos até `C`.
3. `k` linhas `m_t i_1 ... i_m_t`.

Itens são numerados de 1 a `n`. Classes podem se sobrepor, ser vazias ou unitárias; itens podem não pertencer a nenhuma classe. Consumir os `m_t` identificadores e ignorar repetições dentro da mesma classe. Preservar as classes e, se útil, derivar o grafo de conflitos pela união das cliques. Não confundir uma classe original com um grupo SAME da árvore.

### 1.4 Saídas e verificadores

Ambos os arquivos contêm, nesta ordem: valor da incumbente, melhor LB global certificado, número de rotas/recipientes e uma linha por rota/recipiente. Não acrescentar contagem de itens ao início dessas linhas.

- Roteamento (Seção 10.3): linha `t` representa o dia `t`; clientes na ordem de visita, sem depósito. Recalcular deslocamento e prioridade.
- Bin packing (Seção 9.3): valor igual ao número de recipientes; itens em qualquer ordem, mantendo IDs externos `1..n`. Executar `CheckSolution` antes de gravar.
- Verificar cobertura exatamente uma vez, capacidade, conflitos, objetivo informado e `LB <= UB`. Essa última desigualdade sozinha **não prova** que o LB é correto; conferir limites contra enumeração em instâncias pequenas.

## Fase 2 — Geração de colunas e limites certificados

### 2.1 Inicialização e ciclo

- Roteamento: colunas unitárias para cada cliente/dia; construir uma incumbente atribuindo um cliente por dia.
- Bin packing: colunas unitárias e recipientes da heurística fornecida; usar a solução dessa heurística como incumbente inicial.
- Resolver o PMR como LP, extrair duais, executar pricing heurístico e inserir candidatas válidas de custo reduzido negativo. Se nenhuma for encontrada, chamar o exato. Repetir até certificação ou tempo limite.
- Toda coluna deve respeitar as decisões do nó. Deduplicar conjuntos em bin packing e pares sequência/dia em roteamento.
- No bin packing, usar gulosa por valor dual/tamanho, busca local ou aleatorização. Após fusões SAME, operar sobre grupos inteiros. Não remover isoladamente um item com dual negativo de um grupo obrigatório; considerar o valor agregado do grupo.

### 2.2 Limites e interrupção

O valor do PMR incompleto é um **limite superior para o LP completo**, não um LB automaticamente válido para podas. Falha da heurística ou timeout do pricing não certificam ausência de colunas melhorantes. No roteamento, usar o valor do mestre como LB do nó somente após certificação exata para todos os dias, salvo implementação de outro limite demonstrado.

No bin packing (Seção 10.4), iniciar com

$$L_0=\max\left\{\left\lceil\frac{\sum_i s_i}{C}\right\rceil,\max_t|S_t|\right\},$$

com máximo de classes igual a zero se `k=0`. Implementar o **limitante de Farley**:

$$z_{LP}\ge\frac{z_{PMR}}{\alpha},\qquad
\alpha\ge\max\{1,\max_B u(B)\}.$$

Uma cota superior certificada pelo pricing de maximização pode fornecer `alpha`, mesmo se ele não terminar. O valor de uma solução heurística do pricing é uma cota inferior para sua maximização e **não serve** para esse fim. Usar duais ótimos do PMR e a família de colunas admissíveis do nó. Se $\alpha^*\le1$, o mestre convergiu. Arredondar o LB de bin packing para cima com tolerância controlada, por exemplo $\lceil L-10^{-6}\rceil$.

Cada filho herda o LB certificado do pai. Manter na fronteira também o nó em processamento e filhos ainda não processados ao tratar interrupções. O LB global considera todos os subproblemas abertos, limitado pela incumbente, e nunca regride abaixo do LB inicial. Ao esgotar a árvore, gravar `LB = UB`. No roteamento, zero é um LB inicial seguro pelos custos não negativos; não aplicar cotas de número de recipientes a um objetivo de deslocamento/prioridade.

## Fase 3 — Ryan–Foster, viabilidade e arredondamento

### 3.1 Decisões de ramificação

Calcular a fração em que cada par aparece junto:

$$\phi_{ij}=\sum_{B\ni i,j}x_B\quad\text{ou}\quad
\phi_{ij}=\sum_{R\ni i,j}\sum_d x_{R,d}.$$

Selecionar somente pares **fracionários**, $\epsilon<\phi_{ij}<1-\epsilon$, por exemplo o mais próximo de 0,5.

- **SAME:** remover colunas que contêm exatamente um dos dois; pricing seleciona ambos ou nenhum.
- **DIFFERENT:** remover colunas que contêm ambos; acrescentar conflito no pricing.
- Aplicar transitividade de SAME e propagar DIFFERENT entre grupos. Podar grupos que excedem capacidade ou contêm conflitos internos.

Em bin packing, usar uma coluna por conjunto. A Seção 10.5 garante um par fracionário quando o LP é fracionário. Em cada nó, agregar grupos SAME, somando tamanhos e usando seus conflitos. Cada grupo válido sozinho fornece uma coluna inicial: **não são necessárias artificiais** para manter o PMR viável.

No roteamento, preservar as visitas individuais: fundir decisões de seleção não permite substituir clientes por um ponto geométrico único. Uma rota válida contendo cada grupo inteiro, para cada dia, também permite inicializar o mestre sem artificiais. A ordem interna deve ser explicitamente construída e custeada.

**Cuidado específico do roteamento:** colunas distintas podem ter o mesmo conjunto de clientes, mas ordens/dias diferentes. Não transportar automaticamente a prova de existência de par fracionário do bin packing. Se todos os pares são integrais, os conjuntos com suporte positivo formam blocos disjuntos; resolver a atribuição desses blocos aos dias, escolhendo a melhor sequência disponível para cada bloco/dia, recupera uma solução inteira sem piorar o LP convergido. Implementar e testar esse fechamento do nó, evitando ramificar em pares já fixos.

Se uma implementação alternativa usar artificiais, empregar uma fase de viabilidade certificada; não declarar o problema inviável apenas porque o PMR filtrado está inviável, nem aceitar artificiais como solução original.

### 3.2 Busca e arredondamento

Implementar DFS e Best-Bound como estratégias comparáveis. Podar por inviabilidade comprovada, LB certificado que não melhora a incumbente ou solução inteira ótima do nó após convergência.

O arredondamento resolve um mestre inteiro restrito a colunas já geradas. Pode usar o banco global de colunas válidas no problema original, inclusive colunas incompatíveis com o nó atual. Validar a solução antes de atualizar a incumbente; seu resultado fornece **apenas UB**, nunca LB para a árvore completa.

- Roteamento: ao esgotar `-t` sem prova de otimalidade, executar por até `-h` segundos adicionais, se solicitado.
- Bin packing: opcional em algum nó, com orçamento curto **dentro de `T`** (Seção 10.6).

## Fase 4 — Testes e experimentos

- [ ] Exemplos oficiais: roteamento com ótimo 59 e bin packing com ótimo 4, em arquivos separados.
- [ ] Enumeração independente: partições, ordens de visita e dias no roteamento; partições em recipientes no bin packing. Comparar ótimos e limites em dezenas de instâncias pequenas aleatórias.
- [ ] Casos de bin packing: `k=0`, classes sobrepostas, repetições, classes vazias/unitárias, item sem classe, classe com todos os itens e tamanhos iguais a `C`.
- [ ] Casos de roteamento: depósito diferente de zero, prioridades constantes e crescentes, custos não métricos, compactação de dias e fracionamento entre colunas com o mesmo conjunto.
- [ ] Nós SAME/DIFFERENT, conflitos internos, capacidade de grupos, deduplicação, viabilidade após filtragem e interrupção durante pricing.
- [ ] Confirmar orçamento de tempo, solução de saída viável e `LB <= OPT <= UB` usando enumeração.

No **roteamento**, são obrigatórios verificador próprio, enumeração independente, exemplo oficial e logs em `stderr` por nó: LB, incumbente, colunas, chamadas heurísticas/exatas e tempo (Seção 12.4). Manter também para bin packing como critérios de qualidade do projeto; o novo PDF não exige a mesma bateria formal.

As ablações abaixo são obrigatórias no relatório de roteamento (Seção 12.5) e recomendadas para bin packing:

| Estudo | Comparação |
|---|---|
| Pricing | Só exato × heurístico seguido de exato |
| Pré-processamento | Com × sem agregação/propagação, preservando decisões de branching em ambos |
| Par de branching | Mais próximo de 0,5 × outra regra sobre pares fracionários |
| Busca | DFS × Best-Bound |
| Arredondamento | Com × sem, respeitando o orçamento de cada modo |

O modo “só exato” é uma configuração de ablação; a execução normal segue heurística primeiro. Registrar tempo, nós, colunas, LB, UB e gap, com sementes e scripts reprodutíveis. Para UB zero, registrar gap zero se LB também é zero, evitando divisão por zero. No relatório antigo, incluir tabelas, pelo menos um gráfico e discussão dos resultados.

## Fase 5 — Documentação e entrega

### 5.1 Requisitos comuns

- [ ] `codigo/`: fontes C++ e `Makefile`; `make` gera o executável e o caminho do Gurobi é configurável.
- [ ] `testes/`: instâncias e scripts; incluir ferramentas de verificação desenvolvidas.
- [ ] `ia/`: conversas completas, prompts **e respostas**, em ordem cronológica, incluindo esta revisão.
- [ ] `README`: compilação, execução e versões; para bin packing, descrever também os métodos de pricing.
- [ ] Um `.zip` ou `.tar.gz` sem executáveis, objetos, bibliotecas compiladas ou diretórios de build.
- [ ] Trabalho individual, sem compartilhamento de código, texto ou conversas entre alunos; domínio do código entregue.

### 5.2 Exigências específicas dos PDFs

| Item | Roteamento (Seção 12) | Bin packing (Seção 11) |
|---|---|---|
| Destinatário | `fkm@unicamp.br` | `fkm@unicamp.br` |
| Assunto exato | `[PLI] Roteamento com Prioridades e Conflitos - Branch & Price` | `[PLI] Bin Packing com Classes de Conflito` |
| Prazo informado | 15/novembro/2026 | Até duas semanas após divulgação; confirmar a data de divulgação |
| Relatório PDF | Obrigatório | Não listado como entrega obrigatória |
| Histórico Git | Entregar com o código | Não explicitamente exigido; manter como prática de desenvolvimento |
| Ponto de controle e entrevista | Previstos; datas informadas em sala | Não explicitamente previstos no novo PDF |

O relatório de roteamento deve apresentar: algoritmo e estruturas; iteração manual da Seção 8 com duais, duas candidatas e branching; experimentos; pelo menos dois erros concretos da IA e respectivas correções; limitações; resultados do arredondamento `-h`. Não inventar ocorrências para preencher a seção de erros.

A preparação para a entrevista antiga inclui explicar dualidade, pricing, SAME/DIFFERENT, certificação de LB, viabilidade do PMR, arredondamento, duplicatas, tolerâncias e modificar o código ao vivo. Manter esse nível de compreensão também para o novo projeto, sem atribuir a ele requisitos de avaliação ausentes do PDF.

## Sequência de implementação e critérios de conclusão

1. Fixar os dois modelos e exemplos, separando contratos de entrada/saída e objetivo.
2. Preparar parsers e verificadores; integrar e preservar o código fornecido de bin packing.
3. Validar geração de colunas na raiz de cada problema, com certificação exata e Farley no bin packing.
4. Implementar grupos SAME/DIFFERENT, viabilidade, árvore e manutenção do LB global sob timeout.
5. Integrar arredondamento com os dois regimes de tempo; validar contra enumeração.
6. Executar experimentos e preparar cada pacote conforme seu enunciado.

Este roadmap planeja suporte aos dois problemas; sua revisão não implica que os módulos, testes ou entregas já estejam implementados.
