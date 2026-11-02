# Prompt 3:
Modelo: Codex - GPT-6
Data: 02/11/2026

## Prompt 1
Anexado: `enunciado.pdf`.
```
Verifique e descreva as instâncias, a documentação e o parser, de acordo com o enunciado.
```

## Resposta 1
```
O formato oficial da Seção 10.2 foi aplicado ao parser e à instância de teste:

1. n m: número de vértices e arestas de conflito.
2. n linhas com peso, coordenadas e prioridades para os dias 1 a n-1.
3. n(n-1)/2 linhas com os custos c(i,j), para i < j.
4. m linhas com as arestas de conflito.

`tests/instance_zero.txt` foi ajustado ao exemplo oficial e `results/instance_zero.txt` registra o ótimo conhecido, 59. O Roadmap e o TODO foram corrigidos para refletir o enunciado.
```

## Prompt 2
```
Use a estrutura atual para verificar a implementação do parser.
```

## Resposta 2
```
O parser foi mantido dentro de Instance::read. Ele lê as prioridades por dia, constrói a matriz de custos simétrica e carrega os conflitos. O enunciado permite comentários em linhas iniciadas por #; a instância de teste também recebeu comentários explicativos.
```

## Prompt 3
```
O que significa processar o nó raiz e qual é o próximo passo?
```

## Resposta 3
```
Processar o nó raiz significa resolver o mestre restrito inicial e executar geração de colunas até o pricing exato provar que não há custo reduzido negativo. O projeto ainda não chegou a esse ponto: initRoot, o mestre restrito e o pricing continuam com TODOs. Depois do avaliador de rota, o próximo marco é criar as colunas unitárias e o nó raiz.
```

## Correções importantes desta sessão

- Uma resposta inicial do assistente presumiu que as coordenadas determinavam custos Manhattan e descreveu um formato de instância inventado. Isso contradiz o enunciado e foi corrigido após a leitura do PDF.
- O enunciado define custos de deslocamento explícitos c(i,j), prioridades por dia e rotas como sequências ordenadas.
- O parser aceita comentários porque o formato oficial diz que linhas iniciadas por `#` devem ser ignoradas. A instância de teste pode conter comentários explicativos.
