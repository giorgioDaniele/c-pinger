# c-pinger

## Descrizione
**c-pinger** è un semplice programma scritto in linguaggio C che consente di effettuare un ping verso un indirizzo IP specificato e calcolare il round-trip time (RTT) della risposta.

## Requisiti
Per compilare ed eseguire **c-pinger** sono necessari:
- Un compilatore C compatibile con lo standard ANSI C (es. GCC)
- Un sistema operativo Unix-like (Linux, macOS) o Windows

## Compilazione
Per compilare **c-pinger**, eseguire il seguente comando:

```sh
gcc main.c -o ping
```

## Utilizzo
Per eseguire un ping utilizzando **ping**, eseguire il programma fornendo come argomento l'indirizzo IP da pingare. Ad esempio:

```shell
sudo ./ping 8.8.8.8
```

## Output
Dopo aver eseguito il ping, **c-pinger** mostrerà il numero di sequenza del pacchetto inviato, l'RTT della risposta e l'indicatore di successo o fallimento del ping.

## Esempio di output

```shell
ping: 64 bytes with seq = 1 from 8.8.8.8, ttl = 113, rtt = 195.54 ms
ping: 64 bytes with seq = 2 from 8.8.8.8, ttl = 113, rtt = 220.84 ms
ping: 64 bytes with seq = 3 from 8.8.8.8, ttl = 113, rtt = 245.65 ms
ping: 64 bytes with seq = 4 from 8.8.8.8, ttl = 113, rtt = 65.32 ms
ping: 64 bytes with seq = 5 from 8.8.8.8, ttl = 113, rtt = 295.14 ms
ping: 64 bytes with seq = 6 from 8.8.8.8, ttl = 113, rtt = 114.61 ms
```

## Licenza
Questo progetto è senza licenza, senza arte nè parte.
