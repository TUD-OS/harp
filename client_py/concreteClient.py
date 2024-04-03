import logging
import os
import socket
import threading
import time

from client_py.client import Client
# from mappingFeature import MappingFeature
from proto.tetris_pb2 import ClientMessage, ServerResponse, RegistrationRequest


class ConcreteClient(Client):
    def __init__(self, server_socket_path, platform_desc_path, mapping_path):
        super().__init__()
        self._managed = False
        self._logger = logging.getLogger('ConcreteClient')

        try:
            # Read the platform file
            # self._platform = YamlPlatformReader.read_from_file(platform_desc_path)

            # Read the mappings
            # self._mappings = YamlMappingReader.read_mappings(platform=self._platform, file_path=mapping_path)
            # self._logger.debug(f" -> Loaded {len(self._mappings)} mappings for this client")

            # todo: check whether there is a lock even needed in socket-lib-usage
            self._communication_mutex = threading.Lock()

            self._tetris_server_connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self._tetris_server_connection.connect(server_socket_path)
            self._managed = self.register_client()

            print("reached registered client")
            print(self._managed)
            if self._managed:
                print("should be printed")
                # Send over the mappings to the server
                msg = ClientMessage()
                msg.type = ClientMessage.OPERATING_POINTS

                '''
                ops_info = msg.mutable_ops_info()

                for m in self._mappings:
                    op_data = {'identifier': m['name'], 'characteristics': [], 'cpu_ids': m['cpus']}

                    for cn, cv in m['characteristics_map'].items():
                        c = {'name': cn, 'value': cv}
                        op_data['characteristics'].append(c)

                    ops_info['operating_points'].append(op_data)
                '''
                
                response = ServerResponse()
                print("self_managed_reched")
                #self._communication_mutex.acquire()
                #print("lock aquired")
                time.sleep(20)
                self._tetris_server_connection.sendall(msg.SerializeToString())
                print("msg send")
                response_data = self._tetris_server_connection.recv(1024)
                #self._communication_mutex.release()

                if response_data != ServerResponse.ACKNOWLEDGE:
                    self._logger.warning("The server failed to parse our mappings!")
        except Exception as e:
            print("exception thrown: ", e)
            self._logger.info("No TETRiS server, TETRiS is unused.")
            self._managed = False

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
        request.exec = "application-placeholder-name"

        try:
            response = ServerResponse()
            s = request.SerializeToString()
            self._tetris_server_connection.sendall(s)

            print("send checked")
            response_data = self._tetris_server_connection.recv(1024)
            print("recv checked")
            #self._logger.info(f"TETRIS-ID: {response_data['id']}")
            return True
        except Exception as e:
            print("error", e)
            return False

    def close(self):
        self._tetris_server_connection.close()
