#!/bin/bash

# Set variables
IMAGE_NAME="tld_valgrind_image"
CONTAINER_NAME="tld_valgrind_container"
PROJECT_DIR=$(pwd)
BUILD_DIR=${PROJECT_DIR}/build_val

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

# Step 3: Create a build directory if it doesn't exist
mkdir -p ${BUILD_DIR}

# Step 4: Start a new container, run CMake, Make, and CTest
docker run --name ${CONTAINER_NAME} -v ${PROJECT_DIR}:/usr/src/app ${IMAGE_NAME} \
    /bin/bash -c "cmake -DCMAKE_BUILD_TYPE=DEBUG -B/usr/src/app/build_val -H/usr/src/app && make -C /usr/src/app/build_val && make test -C /usr/src/app/build_val "

# Step 5: Stop and remove the container (optional)
echo "Cleaning up..."
docker rm ${CONTAINER_NAME}

echo "Build and memory tests with Valgrind completed!"
