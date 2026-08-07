指令：
openssl genrsa -out cna.key 2048
openssl req -new -x509 -key cna.key -out cna.crt -days 10000
openssl pkcs12 -export -in cna.crt -inkey cna.key -out cna.p12 -name ChartNavigationAndroid

密码：
123