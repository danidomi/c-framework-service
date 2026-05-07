# syntax=docker/dockerfile:1

FROM alpine:3.20 AS builder

RUN apk add --no-cache build-base make

WORKDIR /app
COPY . .

RUN make clean all


FROM alpine:3.20 AS runtime

RUN addgroup -S app && adduser -S app -G app

WORKDIR /app
COPY --from=builder /app/bin/server /app/bin/server

USER app
EXPOSE 8080

CMD ["/app/bin/server"]
