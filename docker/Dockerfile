# Create Alpine image for build
FROM alpine:3.16.2 AS build-image
# Set locale
ENV LC_ALL=C.UTF-8
# Install compiler and standard library
RUN apk add --no-cache gcc musl-dev
# Set working directory
WORKDIR /app/
# Copy source files to working directory
COPY src .
# Compile
RUN gcc -s -O3 -Wall -Wextra -pedantic -DNDEBUG zps.c -o zps
# Create Alpine image for runtime
FROM alpine:3.16.2 AS runtime-image
# Set working directory
WORKDIR /root/
# Copy executable to working directory
COPY --from=build-image /app/zps .
# Execute
CMD ["./zps"]
