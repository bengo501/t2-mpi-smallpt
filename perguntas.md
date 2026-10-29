# perguntas prováveis na apresentação do T2

as respostas usam os números medidos em atlantica03 e atlantica04.
T1 = 48,03 s. entrada 320 x 240, spp = 64, G = 1.
repositório: https://github.com/bengo501/t2-mpi-smallpt

## o que é uma tarefa e por que este problema cabe no mestre/escravo

uma tarefa é um bloco de G linhas. cada pixel é independente e a semente do gerador depende só do número da linha, não de quem renderiza. com G = 1 há 240 tarefas, mais que 63 escravos. ordenar uma linha com path tracing custa 0,263 s; mandar a linha custa 0,08 ms no mesmo nó e 0,35 ms no outro. a computação é centenas de vezes mais cara que a mensagem. foi o mesmo eixo do T1 (a linha da imagem), trocando o parallel for do OpenMP pelo saco de trabalho.

## o que cada tag faz

- TAG_PEDIDO (1): o escravo avisa que está livre. é a primeira mensagem, na linha 257 de smallpt_mpi.c.
- TAG_TAREFA (2): o mestre manda y0, quantas linhas e o id da tarefa.
- TAG_RESULTADO (3): o escravo devolve o RGB. no laço do mestre isso já vale como pedido da próxima.
- TAG_FIM (4): saco vazio. é outra tag, não um valor especial dentro da tarefa. sem ela algum escravo fica bloqueado no recv e o programa não termina.
- TAG_STATS (9): só depois do cronômetro. se a contagem de tarefas fosse enviada com TAG_RESULTADO durante o laço, o MPI_ANY_SOURCE a confundiria com um bloco de pixels.

## por que MPI_ANY_SOURCE e como a ordem original se mantém

o mestre atende quem terminou primeiro. a imagem não pode ser preenchida na ordem de chegada: o centro da cena é mais caro que a borda. task_of[rank] lembra qual id aquele escravo está segurando, porque cada escravo tem no máximo uma tarefa por vez. o bloco é copiado para a linha y0 daquele id. o checksum da primeira cópia bateu com o sequencial nas 140 execuções paralelas. se não batesse, ou o bloco foi parar na linha errada ou o pixel foi calculado por outro caminho.

## por que P = 2 não chega perto de speed-up 2

com 2 processos há 1 escravo. o mestre não renderiza. o speed-up medido é 0,99. a distribuição block coloca esse escravo no outro nó, mas o acréscimo sobre os 48,03 s é de 0,35 s, menos de 1%. a rede não é a causa. a causa é que só um processo calcula. a eficiência pedida pelo enunciado é S/P = 0,50, porque o mestre entra em P. S/(P-1) = 0,99 descreve quem trabalha. na apresentação, dizer as duas e explicar qual o enunciado pede.

## onde o speed-up satura e por quê

antes de medir, o teto é 15: 16 núcleos físicos nos dois nós, um ocupado pelo mestre. em P = 16 o speed-up foi 14,03, 94% desse teto. de P = 4 a P = 16 os valores 2,97, 6,82 e 14,03 acompanham o número de escravos.

P = 32 não dobra. vai de 14,03 para 18,46, ganho de 1,32. cada nó tem 2 threads por núcleo (lscpu: 4 núcleos por soquete, 2 soquetes, 2 threads). hyper-threading não duplica a unidade de ponto flutuante. P = 64 põe dois processos por thread e o speed-up cai para 17,17.

o mestre não é o gargalo. 240 idas e voltas a 0,35 ms dariam 0,08 s, cerca de 3% dos 2,60 s de P = 32. se o mestre serializasse o problema, o tempo teria parado de cair bem antes de P = 16.

## a fraca acompanha a forte. isso é erro?

não. a regra foi --copies = P, a mesma imagem, a mesma semente. os raios primários (4 x 320 x 240 x 64 x copies) crescem exatamente com P. como a comunicação é uma fração pequena do trabalho, T1/Tp fica perto de S/P. até P = 16 a diferença entre as duas eficiências é de no máximo 0,02. as duas quebram juntas no HT: 0,58 e 0,60 em P = 32, 0,27 e 0,30 em P = 64. eficiência fraca acima de 1 seria erro de T1, como no feedback do T1. aqui nenhuma passou de 1.

em P = 32 ainda há cerca de 8 linhas por escravo na forte e cerca de 250 na fraca. a cauda pesa menos na fraca, por isso ela fica um pouco acima, não abaixo.

## granularidade

G = 1 foi a melhor em P = 32 (2,60 s). G = 4 empatou (2,61 s). G = 16 piorou para 5,27 s porque só existem 15 tarefas para 31 escravos: metade não recebe nada. não há G menor que 1 sem partir a linha e a semente. a razão computação/comunicação (3300 no nó, 750 entre nós) justifica não aumentar G para esconder a latência.

## balanceamento no fim do saco

na rodada de P = 32 e G = 1, as cargas foram de 7 a 9 tarefas e o tempo ocupado foi de 2,28 s a 2,60 s. o tempo de parede é o do mais lento. a diferença vem das linhas centrais, que traçam mais raios. com G = 16 essa diferença vira ociosidade: faltam tarefas.

## equivalência

o programa imprime o checksum da primeira imagem e copies_ok. o valor foi 52244.207718229489 em todas as linhas run, igual ao sequencial. copies_ok = 1 significa que as cópias extras são iguais à primeira byte a byte. a geração da câmera e o malloc ficam fora do MPI_Wtime, nos dois programas, nos mesmos limites.

## por que não ler t1 num processo e t2 noutro

MPI_Wtime é o relógio local. com -N 2 os relógios não são o mesmo. t1 e t2 estão os dois no mestre.

## o que o feedback do T1 muda aqui

- equivalência mostrada em toda execução, não afirmada.
- 10 repetições, mediana, mínimo e máximo em todas as tabelas.
- T1 entre 30 e 60 s, mesmas flags, mesmo cluster, sem MPI no sequencial.
- --exclusive e os mesmos dois nós. sem isso outro grupo divide o nó e o speed-up oscila.
- a fraca parte da mesma entrada. o parâmetro que cresce é o número de cópias, e a prova de que o trabalho cresceu é a contagem de raios, não o nome do parâmetro.
- o papel do Amdahl, no mestre/escravo, passa para o custo de comunicação. aqui esse custo foi medido (a sonda) e é pequeno. o limite medido é o núcleo físico e o HT.
- o gráfico principal usa a melhor granularidade (G = 1), não a G = 16 que a própria tabela mostra ser pior.

## se o professor apontar uma chamada MPI

MPI_Recv do mestre: buffer de G x largura x 3 doubles, origem MPI_ANY_SOURCE, tag MPI_ANY_TAG. a tag que volta distingue pedido de resultado. MPI_Send da tarefa: 3 ints. MPI_Send do fim: contagem 0. a barreira antes de t0 só alinha o começo; não entra no algoritmo de distribuição.

## IA

o Cursor (Grok 4.7) escreveu o MPI em cima do smallpt do T1, os scripts e o texto do relatório. as medições e o checksum saíram do cluster. a origem do algoritmo é Kevin Beason, 2008, citada no relatório e no README.
