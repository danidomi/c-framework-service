FROM ubuntu

RUN apt update && apt install -y make zip curl gcc wget

# Create the /app directory
RUN mkdir -p /app

WORKDIR /app

# Continue with your build process
COPY . .

# Compile the binaries
RUN make clean all

EXPOSE 8080

CMD ["/app/bin/server"]