import logging
import os
import socket
import threading

from proto.tetris_pb2 import ClientMessage, ServerResponse, RegistrationRequest, RegistrationResponse, ClientResponse, \
    ServerMessage

from pytetrisclient.client import Client
from pytetrisclient.mappingFeature import MappingFeature
from pytetrisclient.push_message_listener import PushMessageListener
from pytetrisclient.utils.mapping import Mapping
from pytetrisclient.utils.protobufUtil import ProtobufUtil
from pytetrisclient.utils.yamlMappingReader import YamlMappingReader
from pytetrisclient.utils.yamlPlatformReader import YamlPlatformReader
# from mappingFeature import MappingFeature


class ConcreteClient(Client):

    def __init__(self, server_socket_path, platform_desc_path, mapping_path, mapping_coarse_grained):
        super().__init__()
        self._managed = False
        self._logger = logging.getLogger('ConcreteClient')
        self._mapping_coarse_grained = mapping_coarse_grained

        if not self._mapping_coarse_grained:
            self._logger.warning("Fine-graned mappings are not fully supported.")

        self._push_message_listener = PushMessageListener(self.__get_push_listener_socket_path(), self)

        try:
            # Read the platform file
            self._platform = YamlPlatformReader.read_platform(platform_desc_path)

            # Read the mappings
            self._mappings, app_name = YamlMappingReader.read_mappings(file_path=mapping_path, platform=self._platform)
            self._active_mapping = None

            self._mapping_features = []
            # self._logger.debug(f" -> Loaded {len(self._mappings)} mappings for this client")

            # todo: check whether there is a lock even needed in socket-lib-usage
            self._communication_mutex = threading.Lock()

            self._tetris_server_connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self._tetris_server_connection.connect(server_socket_path)
            self._managed = self.register_client(app_name)

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
        ops_info = ClientMessage.OperatingPointsInfo()
        for mapping in mappings:
            op = ops_info.operating_points.add()
            op.identifier = mapping.name
            op.cpu_ids.extend(mapping.cpu_ids)
            op.utility = mapping.characteristics.get("utility", 0.0)
            op.power = mapping.characteristics.get("power", 0.0)
        msg.ops_info.CopyFrom(ops_info)

    @staticmethod
    def __get_push_listener_socket_path():
        return f"/tmp/tetris_push_listener_{os.getpid()}"

    def bind(self, feature):
        # to implement if needed
        # but not in use atm.
        raise NotImplementedError("Implement method if needed!")

    def send(self, msg: ClientMessage) -> ServerResponse:
        # to implement if needed
        # but not in use atm.
        raise NotImplementedError("Implement method if needed!")

    def bind_mapping_feature(self, feature: MappingFeature):
        if not self._managed:
            return

        feature.accept(self)

        if feature.need_handshake():
            feature_id = feature.handshake()
            self._push_message_listener.add_subscriber(feature_id, feature)

        self._mapping_features.append(feature)

        if self._active_mapping:
            # todo: implement
            raise NotImplementedError("implement mapping update with conv-map")
            feature.mapping_update(self._active_mapping, {})

    def handle(self, msg: ServerMessage) -> ClientResponse:
        response = ClientResponse()
        response.type = ClientResponse.Type.ERROR

        if msg.type == ServerMessage.ACTIVATE_CUSTOM_OP:
            if msg.HasField('activated_op_info'):
                active_op = msg.activated_op_info
                map_id = active_op.identifier
                print(f" * Got mapping update from server: {map_id}")

                conv_map = {conv.cpu_id_from: conv.cpu_id_to for conv in active_op.cpu_convs}

                # Search for the mapping with the given map_id
                mapping = next((m for m in self._mappings if m.name == map_id), None)

                if mapping:
                    self._active_mapping = mapping.convert(conv_map)
                    print(f" -> Active mapping {self._active_mapping.name}")

                    # Tell the features to react to the new mapping
                    for feature in self._mapping_features:
                        feature.mapping_update(self._active_mapping, conv_map)

                    response.type = ClientResponse.Type.ACKNOWLEDGE
        elif msg.type == ServerMessage.ACTIVATE_CPUS:
            if msg.HasField('activated_cpus'):
                cpu_ids = list(msg.activated_cpus.cpu_ids)
                name = str(cpu_ids)
                mapping = Mapping(name, cpu_ids, {})

                # Tell the features to react to the new mapping
                for feature in self._mapping_features:
                    feature.mapping_update(mapping, {})

                response.type = ClientResponse.Type.ACKNOWLEDGE
        else:
            self._logger.error("Unknown server message type.")


        return response

    def register_client(self, app_name):
        request = RegistrationRequest()

        request.pid = os.getpid()
        request.exec = app_name
        if self._mapping_coarse_grained:
            request.mapping_type = RegistrationRequest.COARSE_GRAINED
        else:
            request.mapping_type = RegistrationRequest.FINE_GRAINED

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
        self._push_message_listener.close()
