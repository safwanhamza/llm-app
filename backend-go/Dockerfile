FROM golang:1.21-alpine AS builder

WORKDIR /app

# Copy go mod/sum first
COPY backend-go/go.mod .
# Run go mod tidy later to fill dependencies
# COPY backend-go/go.sum . # Not yet created

# Copy generated protos
COPY backend-go/pb /app/backend-go/pb

# Copy source
COPY backend-go/src /app/backend-go/src

# Fix import paths in source if needed or ensure module structure is correct
# We initialized the module as github.com/simulation-app/backend-go
# The source file is at backend-go/src/main.go but imports local pb
# We need to organize it properly

WORKDIR /app/backend-go
RUN go mod tidy

WORKDIR /app/backend-go/src
# Build the binary
RUN go build -o /app/backend-server main.go

FROM alpine:latest
WORKDIR /app
COPY --from=builder /app/backend-server .
RUN mkdir data

EXPOSE 8080

CMD ["./backend-server"]
