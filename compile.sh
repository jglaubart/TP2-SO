# Validates the existance of the TP2-SO container, starts it up & compiles the project
CONTAINER_NAME="TP2-SO"

# Memory allocator selection (default: buddy)
ALLOCATOR="${1:-buddy}"

if [ "$ALLOCATOR" != "buddy" ] && [ "$ALLOCATOR" != "bitmap" ]; then
    echo "${RED}Invalid allocator. Use 'buddy' or 'bitmap'. Defaulting to 'buddy'.${NC}"
    ALLOCATOR="buddy"
fi

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
    docker pull agodio/itba-so:2.0
    echo "Creating container..."
    # Note: ${PWD}:/root. Using another container to compile might fail as the compiled files would not be guaranteed to be at $PWD
    # Always use TP2-SO to compile
    docker run -d -v ${PWD}:/root --security-opt seccomp:unconfined -it --name "$CONTAINER_NAME" agodio/itba-so:2.0
    echo "${GREEN}Container $CONTAINER_NAME created.${NC}"
else
    echo "${GREEN}Container $CONTAINER_NAME exists.${NC}"
    # Verify the container actually has the repository mounted at /root
    docker exec "$CONTAINER_NAME" test -f /root/Makefile &> /dev/null
    if [ $? -ne 0 ]; then
        echo "${YELLOW}Container $CONTAINER_NAME does not contain the repository mounted at /root. Recreating container with volume...${NC}"
        # Remove the old container and create a new one with the correct mount
        docker rm -f "$CONTAINER_NAME" &> /dev/null || true
        docker run -d -v ${PWD}:/root --security-opt seccomp:unconfined -it --name "$CONTAINER_NAME" agodio/itba-so:2.0
        echo "${GREEN}Container $CONTAINER_NAME recreated with repository mounted.${NC}"
    fi
fi

# Start container
docker start "$CONTAINER_NAME" &> /dev/null
echo "${GREEN}Container $CONTAINER_NAME started.${NC}"

# ========================
# PVS-STUDIO (opcional)
# Uso: ./compile.sh [buddy|bitmap] [--pvs]
# Genera /root/pvs-report (montado a ./pvs-report en host)
# ========================
DO_PVS=0
if [[ "$2" == "--pvs" ]] || [[ "$1" == "--pvs" ]]; then
  DO_PVS=1
fi

if [ $DO_PVS -eq 0 ]; then
  # Compila normal (como siempre)
  echo "${YELLOW}Compiling with ${ALLOCATOR} memory allocator...${NC}"
  docker exec -it "$CONTAINER_NAME" make clean -C /root/ && \
  docker exec -it "$CONTAINER_NAME" make all -C /root/Toolchain && \
  docker exec -it "$CONTAINER_NAME" make all -C /root/ ALLOCATOR="$ALLOCATOR"
else
  echo "${YELLOW}Running build under PVS-Studio trace with ${ALLOCATOR} memory allocator...${NC}"
  docker exec -it "$CONTAINER_NAME" bash -lc '
    set -e
    # 1) Instalar PVS-Studio si hace falta (Debian/Ubuntu)
    if ! command -v pvs-studio-analyzer >/dev/null 2>&1; then
      echo "Installing PVS-Studio..."
      apt-get update && apt-get install -y wget gnupg2 lsb-release
      wget -q -O - https://pvs-studio.com/files/keys/ABF3B1EA.key | apt-key add -
      echo "deb http://repo.pvs-studio.com/$(lsb_release -is | tr "[:upper:]" "[:lower:]") $(lsb_release -cs) main" > /etc/apt/sources.list.d/pvs-studio.list
      apt-get update && apt-get install -y pvs-studio
      echo "PVS-Studio installed successfully."
    else
      echo "PVS-Studio already installed."
    fi

    cd /root
    rm -f strace_out PVS-Studio.log
    
    # 2) Trazamos toda la build (clean + toolchain + proyecto)
    echo "Tracing build with PVS-Studio..."
    pvs-studio-analyzer trace -- /bin/bash -lc "
      make clean -C /root/ &&
      make all -C /root/Toolchain &&
      make all -C /root ALLOCATOR='"$ALLOCATOR"'
    "
    
    # 3) Analizamos el código
    echo "Analyzing code with PVS-Studio..."
    # Para proyectos open-source, usar credenciales FREE
    pvs-studio-analyzer credentials PVS-Studio Free FREE-FREE-FREE-FREE 2>/dev/null || true
    pvs-studio-analyzer analyze -f strace_out -o /root/PVS-Studio.log
    
    # 4) Generamos reporte HTML
    echo "Generating HTML report..."
    rm -rf /root/pvs-report
    plog-converter -a GA:1,2 -t fullhtml -o /root/pvs-report /root/PVS-Studio.log
    
    echo "PVS-Studio analysis complete!"
  '
fi

if [ $? -ne 0 ]; then
    echo "${RED}Compilation or analysis failed.${NC}"
    exit 1
fi

echo "${GREEN}Done.${NC}"
if [ $DO_PVS -eq 1 ]; then
    echo "${GREEN}PVS-Studio report generated at: ./pvs-report/index.html${NC}"
    echo "${YELLOW}Open it in your browser to see the analysis results.${NC}"
else
    echo "${GREEN}Compilation finished.${NC}"
fi
