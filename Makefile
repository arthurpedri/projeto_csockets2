all:
	gcc -pthread cliente.c -o talisker
	gcc -pthread priorat.c -o priorat
	gcc -pthread bowmore.c -o bowmore
	gcc -pthread cohiba.c -o cohiba
