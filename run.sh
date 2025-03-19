# Validates the existance of the TPE-ARQ container, starts it up & compiles the project
CONTAINER_NAME="TPE-SO-2025-g02"

# COLORS
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m'

docker ps -a &> /dev/null

if [ $? -ne 0 ]; then
    echo "${RED}Docker is not running. Please start Docker and try again.${NC}"
    exit 1
fi

# Check if container exists
if [ ! "$(docker ps -a | grep "$CONTAINER_NAME")" ]; then
    echo "${YELLOW}Container $CONTAINER_NAME does not exist. ${NC}"
    echo "Pulling image..."
    docker pull agodio/itba-so-multi-platform:3.0
    echo "Creating container..."
    # Note: ${PWD}:/root. Using another container to compile might fail as the compiled files would not be guaranteed to be at $PWD
    # Always use TPE-ARQ to compile
    docker run -d -v ${PWD}:/root --security-opt seccomp:unconfined -it --name "$CONTAINER_NAME" agodio/itba-so-multi-platform:3.0
    echo "${GREEN}Container $CONTAINER_NAME created.${NC}"
else
    echo "${GREEN}Container $CONTAINER_NAME exists.${NC}"
fi

# Start container
docker start "$CONTAINER_NAME" &> /dev/null

if [ $? -ne 0 ]; then
    echo "${RED}Failed.${NC}"
    exit 2
else
    echo "${GREEN}Container $CONTAINER_NAME started.${NC}"
fi


# Get into a shell within the container
docker exec -it "$CONTAINER_NAME" /bin/bash

if [ $? -ne 0 ]; then
    echo "${RED}Previous command failed, container was subsequently exited.${NC}"
    exit 3
else
    echo "${YELLOW}Exited container.${NC}"
fi