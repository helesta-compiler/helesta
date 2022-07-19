# Error: selected processor does not support `vldr' in ARM mode
docker build . -t helesta:v1
docker run -it -v $(pwd)/output:/root/output -v $(pwd)/testcases/公开样例与运行时库:/root/testcases --rm helesta:v1