import socket
import threading
from typing import Dict

from client_py.feature import Feature
from client_py.utils.protobufUtil import ProtobufUtil
from proto.tetris_pb2 import ServerMessage, ClientResponse


class PushMessageListener:
    def __init__(self, socket_path: str, client):
        self._listener_thread = threading.Thread(target=self.listening)
        self._client = client

        self._listening_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._listening_socket.bind(socket_path)
        self._listening_socket.listen()
        self._listening = True

        self._listener_thread.start()

        self._subscribers: Dict[int, Feature] = {}

    def add_subscriber(self, feature_id: int, feature):
        self._subscribers[feature_id] = feature

    def forward(self, msg: ServerMessage) -> ClientResponse:
        feature_id = msg.feature_id
        if feature_id == 0:
            return self._client.handle(msg)
        else:
            feature = self._subscribers.get(feature_id)
            if feature:
                return feature.handle(msg)
            else:
                raise ValueError(f"No subscriber found for feature ID: {feature_id}")

    def listening(self):
        while True:
            conn, _ = self._listening_socket.accept()
            if not self._listening:
                break
            msg = ServerMessage()
            ProtobufUtil.receive(conn, msg)
            response = self.forward(msg)
            ProtobufUtil.send(conn, response)

    def close(self):
        self._listening = False
        self._listening_socket.send(str.encode("placeholder_string_to_abort"))
        self._listening_socket.close()
        self._listener_thread.join()
