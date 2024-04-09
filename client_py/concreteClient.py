import logging
import os
import socket
import threading

from client_py.client import Client
from client_py.push_message_listener import PushMessageListener
from client_py.utils.protobufUtil import ProtobufUtil
from client_py.utils.yamlMappingReader import YamlMappingReader
# from mappingFeature import MappingFeature
from proto.tetris_pb2 import ClientMessage, ServerResponse, RegistrationRequest, RegistrationResponse


class ConcreteClient(Client):
    def __init__(self, server_socket_path, platform_desc_path, mapping_path):
        super().__init__()
        self._managed = False
        self._logger = logging.getLogger('ConcreteClient')
        self._push_message_listener = PushMessageListener(self.__get_push_listener_socket_path(), self)

        try:
            # Read the platform file
            # self._platform = YamlPlatformReader.read_from_file(platform_desc_path)

            # Read the mappings
            self._mappings = YamlMappingReader.read_mappings(file_path=mapping_path)
            # self._logger.debug(f" -> Loaded {len(self._mappings)} mappings for this client")

            # todo: check whether there is a lock even needed in socket-lib-usage
            self._communication_mutex = threading.Lock()

            self._tetris_server_connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self._tetris_server_connection.connect(server_socket_path)
            self._managed = self.register_client()

            # print(self._managed)
            if self._managed:
                # Send available mappings to the server
                msg = ClientMessage()
                msg.type = ClientMessage.OPERATING_POINTS
                self.__add_mappings_to_client_message(self._mappings, msg)

                ProtobufUtil.send(self._tetris_server_connection, msg)
                response = ServerResponse()
                ProtobufUtil.receive(self._tetris_server_connection, response)

                if response.type != ServerResponse.ACKNOWLEDGE:
                    self._logger.warning("The server failed to parse our mappings!")
                # self._logger.info("Server connection established. Operating points were sent successfully")
        except Exception as e:
            print("exception thrown: ", e)
            self._logger.info("No TETRiS server, TETRiS is unused.")
            self._managed = False

    @staticmethod
    def __add_mappings_to_client_message(mappings, msg):
        for mapping in mappings:
            op = msg.ops_info.operating_points.add()
            op.identifier = mapping.name
            op.cpu_ids.extend(mapping.cpu_ids)
            for name, value in mapping.characteristics.items():
                characteristic = op.characteristics.add()
                characteristic.name = name
                characteristic.value = value

    @staticmethod
    def __get_push_listener_socket_path():
        return f"/tmp/tetris_push_listener_{os.getpid()}"

    def bind(self, feature):
        # to implement
        pass

    def bind_mapping_feature(self, mapping_feature):
        # to implement
        pass

    def send(self, message):
        pass

    def handle(self, msg):
        pass

    def register_client(self):
        request = RegistrationRequest()

        request.pid = os.getpid()
        request.exec = os.path.realpath(__file__)

        try:
            ProtobufUtil.send(self._tetris_server_connection, request)
            response = RegistrationResponse()
            ProtobufUtil.receive(self._tetris_server_connection, response)
            print(f"TETRIS-ID: {response.id}")
            return True
        except Exception as e:
            print("error", e)
            return False

    def close(self):
        self._tetris_server_connection.close()
