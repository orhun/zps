FROM alpine:3.10.2 AS build-image
RUN apk add --no-cache gcc musl-dev
ENV LC_ALL=C.UTF-8
WORKDIR /app/
COPY src .
RUN gcc -O3 -Wall zps.c -o zps

FROM alpine:3.10.2 AS runtime-image
WORKDIR /root/
COPY --from=build-image /app/zps .
CMD ["./zps"]