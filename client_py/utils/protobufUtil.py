class ProtobufUtil:
    @staticmethod
    def receive(socket_connection, msg):
        # expect 4 byte int representing the size of following message
        size_of_data = socket_connection.recv(4)
        raw_data = socket_connection.recv(size_of_data)
        if len(raw_data) > 0:
            msg.ParseFromString(raw_data)
        else:
            raise RuntimeError("Read failed!")

    @staticmethod
    def send(socket_connection, msg):
        # Serialize message to bytes
        raw_data = msg.SerializeToString()
        socket_connection.sendall(len(raw_data).to_bytes())
        socket_connection.sendall(raw_data)
