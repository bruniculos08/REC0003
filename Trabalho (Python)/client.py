import socket
import os

#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#Obs: arrumar codigo para que op upload e download possam enviar arquivos de tamanho indeterminado, dividindo o arquivo em pacotes de 1024

# Configuração do cliente local e LAN
# host = "127.0.0.1"
# port = 63724

#config cliente WAN
# host = "0.tcp.sa.ngrok.io"
# port = 14953

#config geral do cliente

host = input("Digite o host:")
port = input("DIgite a porta:")
port = int(port)
print(host, port)

# Criação do socket
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Conexão ao servidor
client_socket.connect((host, port))
print(f"Conectado ao servidor em {host}:{port}")

try:
    while True:
        # Comandos disponíveis
        print("Comandos disponíveis: list, upload, download, delete, exit")
        
        # Recebe o comando do usuário
        command = input("Digite um comando: ")

        if command == "exit":
            # Envia o comando ao servidor
            client_socket.send(command.encode())
            # Se o cliente solicitar encerramento, saia do loop
            break

        elif command == "list":
            # Envia o comando ao servidor
            client_socket.send(command.encode())

            # Processa a resposta do servidor para outros comandos
            response = client_socket.recv(1024).decode()
            print("Arquivos disponíveis:", response)

        elif command.startswith("upload"):
            # Verifica se o comando upload contém um argumento
            if len(command.split()) == 2:
                # Se o comando for "upload", o cliente enviará o conteúdo do arquivo
                filename = command.split()[1]
                try:
                    with open(filename, "rb") as file:
                        file_data = file.read()

                        # Envia o comando ao servidor
                        client_socket.send(command.encode())

                        #envia o arquivo
                        chunk_size = 1024
                        for i in range(0, len(file_data), chunk_size):
                            client_socket.send(file_data[i:i+chunk_size])
                        client_socket.send("FIM_ARQUIVO".encode())

                        # Processa a resposta do servidor para outros comandos
                        response = client_socket.recv(1024).decode()
                        print(response)

                except FileNotFoundError:
                    print(f"Arquivo {filename} não encontrado.")
            else:
                print("Comando upload requer um argumento (nome do arquivo).")

        elif command.startswith("download"):
            # Verifica se o comando upload contém um argumento
            if len(command.split()) == 2:
                # Envia o comando ao servidor
                client_socket.send(command.encode())

                response = client_socket.recv(1024).decode()
                if response != "Enviando arquivo...#%$$%#":
                    response = response.split("#%$$%#")

                    if response[0] == "Enviando arquivo...":
                        # Se o comando for "download", o servidor enviará o conteúdo do arquivo
                        print(response[0])

                        # Obtém o diretório atual do script
                        current_directory = os.path.dirname(os.path.abspath(__file__))

                        with open(os.path.join(current_directory, command.split()[1]), "wb") as file:
                            # file_data = client_socket.recv(1024)
                            file_data = response[1]
                            print(file_data.endswith("FIM_ARQUIVO"))
                            if not file_data.endswith("FIM_ARQUIVO"):
                                print("nice")
                                while not file_data.endswith("FIM_ARQUIVO"):
                                    file_data += client_socket.recv(1024).decode()
                            
                            #remove o FIM_ARQUIVO
                            file_data = file_data[:-11]

                            file_data.encode()
                            file.write(file_data)
                            print("Arquivo baixado com sucesso!")
                    else:
                        print(response)
                else:
                    if response == "Enviando arquivo...#%$$%#":
                        # Se o comando for "download", o servidor enviará o conteúdo do arquivo
                        response = response.split("#%$$%#")
                        print(response[0])

                        # Obtém o diretório atual do script
                        current_directory = os.path.dirname(os.path.abspath(__file__))

                        with open(os.path.join(current_directory, command.split()[1]), "wb") as file:
                            # file_data = client_socket.recv(1024)
                            file_data = response[1]
                            if not file_data.endswith("FIM_ARQUIVO"):
                                while not file_data.endswith("FIM_ARQUIVO"):
                                    file_data += client_socket.recv(1024).decode()
                            
                            #remove o FIM_ARQUIVO
                            file_data = file_data[:-11]

                            file_data = file_data.encode()

                            file.write(file_data)
                            print("Arquivo baixado com sucesso!")
                    else:
                        print(response)
            else:
                print("Comando download requer um argumento (nome do arquivo).")
                

        elif command.startswith("delete"):
            # Verifica se o comando upload contém um argumento
            if len(command.split()) == 2:
                # Envia o comando ao servidor
                client_socket.send(command.encode())
                response = client_socket.recv(1024).decode()
                print(response)
            else:
                print("Comando delete requer um argumento (nome do arquivo).")
    
        else: 
            print("Comando não encontrado")
       
except Exception as e:
    print(f"Erro: {e}")
finally:
    # Fecha a conexão
    client_socket.close()
    print("Conexão encerrada.")