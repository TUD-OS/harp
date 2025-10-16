from abc import ABC, abstractmethod

from proto.tetris_pb2 import ClientMessage, ClientResponse, ServerMessage, ServerResponse

from pytetrisclient.feature import Feature
from pytetrisclient.mappingFeature import MappingFeature


class Client(ABC):
    """
    TETRiS client interface.
    """

    @abstractmethod
    def bind(self, feature: Feature):
        """
        Binds a TETRiS feature to the TETRiS client.

        The binding procedure includes handshaking with the TETRiS server if the feature needs to subscribe to
        push notifications.

        :param feature: Pointer to the TETRiS feature.
        """
        pass

    @abstractmethod
    def bind_mapping_feature(self, mapping_feature: MappingFeature):
        """
        Binds a TETRiS Mapping feature to the TETRiS client.

        Mapping features are special extensions of clients that allow the client to do more advanced mapping
        changes. These features will be activated whenever the mapping of the client changes. If necessary,
        they might register also with the TETRiS server.

        :param mapping_feature: Pointer to the TETRiS Mapping feature.
        """
        pass

    @abstractmethod
    def send(self, msg: ClientMessage) -> ServerResponse:
        """
        Sends a client request to the TETRiS server.

        :param msg: Client request to send.
        :return: Response from the TETRiS server.
        """
        pass

    @abstractmethod
    def handle(self, msg: ServerMessage) -> ClientResponse:
        """
        Handle messages from the TETRiS server that need to be directly handled by the client
        (send with featureID 0).

        :param msg: Message from the server that needs to be handled.
        :return: Response that should be sent back to the server.
        """
        pass
