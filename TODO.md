# TODO

## Instância e parser

- [x] Seguir o formato da Seção 10.2 do `enunciado.pdf`.
- [x] Ler comentários iniciados por `#`.
- [x] Ler prioridades por dia, custos de deslocamento `c(i,j)` e conflitos.
- [x] Validar quantidade de campos, índices, custos, conflitos e conteúdo extra.
- [x] Converter `tests/instance_zero.txt` para o exemplo oficial da Seção 8.
- [ ] Conferir a execução do parser com `tests/instance_zero.txt` (`-s 0 -C 10`).

## Próximo passo: avaliar uma sequência de rota

`Instance::evaluateRoute(route, day)` recebe uma sequência ordenada de clientes (sem o depósito). Ao completar o método:

- [ ] Validar o dia entre `1` e `n - 1`, a rota não vazia, os índices, a ausência de repetição e a ausência do depósito.
- [ ] Somar os pesos da sequência e rejeitar capacidade excedida.
- [ ] Rejeitar pares de clientes que aparecem no grafo de conflitos.
- [ ] Calcular o deslocamento somando a matriz `c` na ordem recebida, do depósito à rota e de volta.
- [ ] Calcular a prioridade somando `pi(i, day)` de cada cliente.
- [ ] Preencher `RouteEvaluation`; para rota inválida, manter `valid == false`.

Não calcular custos por Manhattan nem permutar a sequência: as coordenadas são apenas para visualização, e a ordem faz parte da definição da rota.
