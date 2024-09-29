#!/bin/bash

# Set variables
IMAGE_NAME="tld_valgrind_image"
CONTAINER_NAME="tld_valgrind_container"
PROJECT_DIR=$(pwd)

# Step 1: Check if the Docker image exists
if [[ "$(docker images -q ${IMAGE_NAME} 2> /dev/null)" == "" ]]; then
    echo "Docker image not found. Building the image..."
    docker build -t ${IMAGE_NAME} .
else
    echo "Docker image found. Skipping build."
fi

# Step 2: Stop any existing container
if [[ "$(docker ps -q -f name=${CONTAINER_NAME})" ]]; then
    echo "Stopping existing container..."
    docker stop ${CONTAINER_NAME}
fi

# Step 3: Start a new container, run CMake, Make, and tests directly
docker run --name ${CONTAINER_NAME} -v ${PROJECT_DIR}:/usr/src/app ${IMAGE_NAME} \
    /bin/bash -c "cmake /usr/src/app && make && make test"

# Step 4: Stop and remove the container (optional)
echo "Cleaning up..."
docker rm ${CONTAINER_NAME}

echo "Build and tests completed!"
