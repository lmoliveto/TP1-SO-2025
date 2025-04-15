# TPE-SO-2025



## Docker

To start docker simply run 

```sh
# chmod a+x ./run.sh
./run.sh
```

## Compile

Within the root folder of the docker container, compile this project using make:

```sh
# cd root
make all
```

## Run

To run the program simply use one of the compiled executables as such:

```sh
# Starts a game with a view and three players
./Master -v view -p player player_up player_neighb
```
