from abc import ABC
from typing import Dict

from proto.tetris_pb2 import ClientMessage, ClientResponse, ServerMessage

from pytetrisclient.feature import Feature
from pytetrisclient.utils.mapping import Mapping


class MappingFeature(Feature, ABC):
    """
    TETRiS Mapping Feature abstract class.

    A TETRiS Mapping feature is an extension of the normal mapping procedure that allows clients to dynamically extend
    mapping information before they are sent to the TETRiS server and also react to potential mapping changes of the
    client's mapping in order to implement more complex mapping features.
    """

    def __init__(self):
        super().__init__()

    def mapping_update(self, mapping: Mapping, conv: Dict[int, int]):
        """
        Handle mapping changes for the client.

        When the client gets an updated mapping info, handle the changes accordingly.

        :param mapping: The updated and converted mapping received from the TETRiS server.
        :param conv: The conversion map that contains the information on which CPUs should be used.
                     The conversion map is already applied to the given mapping argument.
        """
        pass

    def extend_mapping(self, mappings: ClientMessage.OperatingPointsInfo) -> bool:
        """
        Extend mapping information before sending them to the TETRiS server.

        Update or extend mapping information before the client sends them to the TETRiS server.

        :param mappings: Current state of the mapping information.
        :return: True if mapping information was updated, False otherwise.
        """
        return False

    def handle(self, msg: ServerMessage) -> ClientResponse:
        """
        Provide default implementation for the original Feature interface for MappingFeatures
        that don't want to communicate with the server.

        :param msg: Message from the server that needs to be handled.
        :return: Response that should be sent back to the server.
        """
        raise NotImplementedError("Not implemented!")

    def handshake(self) -> int:
        """
        Provide default implementation for handshake.

        :return: Allocated feature ID.
        """
        raise NotImplementedError("Not implemented!")
