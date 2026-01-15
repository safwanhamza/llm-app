FROM python:3.9-slim

WORKDIR /app

COPY ai-python/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy protos (relative to build context)
COPY protos/ /app/protos/

# Copy generated code directory structure (it will be regenerated or copied)
# Ideally we generate it inside the container to match the environment
RUN pip install grpcio-tools

# Generate protos inside the container
RUN mkdir -p ai-python/generated
RUN python -m grpc_tools.protoc -I. --python_out=./ai-python/generated --grpc_python_out=./ai-python/generated protos/simulation.proto protos/ai.proto

# Copy source
COPY ai-python/src /app/ai-python/src

# Create an __init__.py for generated/protos package so python can import it
RUN touch /app/ai-python/generated/protos/__init__.py

ENV PYTHONPATH=/app/ai-python/src:/app/ai-python/generated

CMD ["python", "ai-python/src/main.py"]
