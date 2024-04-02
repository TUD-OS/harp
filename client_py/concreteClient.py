import threading
from client import Client
from feature import Feature
from mappingFeature import MappingFeature
from proto.tetris_pb2 import ClientMessage, ClientResponse, ServerMessage, ServerResponse


class ConcreteClient(Client):
    def __init__(self, server_socket_path, platform_desc_path, mapping_path):
        super().__init__()
        self._managed = False
        self._logger = logging.getLogger('ConcreteClient')

        try:
            # Read the platform file
            self._platform = YamlPlatformReader.ReadFromFile(platform_desc_path)

            # Read the mappings
            self._mappings = YamlMappingReader.read_mappings(self._platform, mapping_path)
            self._logger.debug(f" -> Loaded {len(self._mappings)} mappings for this client")
            self._communication_mutex = threading.Lock()
            self._tetris_server_connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self._tetris_server_connection.connect(server_socket_path)
            self._managed = self.register_client()

            if self._managed:
                # Send over the mappings to the server
                msg = ClientMessage()
                msg.set_type(ClientMessage.OPERATING_POINTS)
                ops_info = msg.mutable_ops_info()

                for m in self._mappings:
                    op_data = {'identifier': m['name'], 'characteristics': [], 'cpu_ids': m['cpus']}

                    for cn, cv in m['characteristics_map'].items():
                        c = {'name': cn, 'value': cv}
                        op_data['characteristics'].append(c)

                    ops_info['operating_points'].append(op_data)

                response = ServerResponse()
                self._communication_mutex.acquire()
                self._tetris_server_connection.sendall(msg)
                response_data = self._tetris_server_connection.recv(1024)
                self._communication_mutex.release()

                if response_data != ServerResponse.ACKNOWLEDGE:
                    self._logger.warning("The server failed to parse our mappings!")
        except Exception as e:
            self._logger.info("No TETRiS server, TETRiS is unused.")
            self._managed = False

    def bind(self, feature):
        # to implement
        pass

    def bind_mapping_feature(self, mapping_feature: MappingFeature):
        # to implement
        pass


    def send(self, message):
        response = ServerResponse()
        self._communication_mutex.acquire()
        self._tetris_server_connection.sendall(message)
        response_data = self._tetris_server_connection.recv(1024)
        self._communication_mutex.release()
        return response

    def handle(self, msg):
        response = {'type': 'ERROR'}

        if 'activated_op_info' in msg:
            active_op = msg['activated_op_info']

            map_id = active_op['identifier']
            self._logger.info(f" * Got mapping update from server: {map_id}")

            conv_map = {conv['cpu_id_from']: conv['cpu_id_to'] for conv in active_op['cpu_convs']}

            mapping = next((m for m in self._mappings if m['name'] == map_id), None)

            if mapping:
                self._active_mapping = {'name': map_id, 'characteristics_map': mapping['characteristics_map'],
                                        'cpus': mapping['cpus']}
                self._logger.debug(f" -> Active mapping {self._active_mapping['name']}")

                for feature in self._mapping_features:
                    feature.mapping_update(self._active_mapping, conv_map)

                response['type'] = 'ACKNOWLEDGE'

        return response

    def register_client(self):
        request = {'pid': os.getpid(), 'exec': subprocess.check_output(["readlink", "/proc/self/exe"]).decode().strip()}

        try:
            response = ServerResponse()
            self._socket.connect(server_socket_path)
            self._socket.sendall(request)
            response_data = self._communication_mutex.recv(1024)

            self._logger.info(f"TETRIS-ID: {response_data['id']}")
            return True
        except Exception as e:
            return False

    def close(self):
        self._communication_mutex.close()
