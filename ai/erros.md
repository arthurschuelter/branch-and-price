# Erros

## Erro 1
```
Em pricing.cpp (src/core/pricing.cpp:214), cada chamada a Pricer::exact() cria um ambiente e um modelo Gurobi. Dentro dessa chamada, o código reutiliza o mesmo modelo: altera o objetivo para cada dia e chama model.optimize() novamente. Por isso o cabeçalho do otimizador aparece várias vezes. As mensagens MIP start from previous solve mostram que o Gurobi também tentou aproveitar uma solução da resolução anterior.

Ao final, a enumeração independente calculou LB inteiro 59, que igualou a incumbente UB 59. É essa igualdade que permite fechar a raiz e provar a otimalidade inteira do exemplo. A mensagem de que a convergência do LP não está certificada continua correta: são duas conclusões distintas.

Criar um novo ambiente a cada chamada do pricing pode ter custo de desempenho, especialmente em instâncias maiores ou com muitos nós. Reutilizar o ambiente entre chamadas é uma possível melhoria, mas as seis otimizações por dia continuam sendo esperadas nessa formulação
```