import logging

from proto.tetris_pb2 import ClientMessage, ServerResponse, ClientResponse, ServerMessage, FeatureInfo

from pytetrisclient.client import Client
from pytetrisclient.utils.protobufUtil import ProtobufUtil
from pytetrisclient.feature import Feature


class UtilityMeasure(Feature):
    def __init__(self):
        super()._init__()

        self._utility_measures = list()

    def handle(self, msg: ServerMessage):
        response = ClientResponse()

        if len(self._utility_measures) > 0:
            response.type = ClientResponse.UTILITY_UPDATE
            response.utility = sum(self._utility_measures)/len(self._utility_measures)
            self._utility_measures.clear()
        else:
            response.type = ClientResponse.UTILITY_RETRY

        return response

    def need_handshake(self):
        return True

    def handshake(self):
        msg = ClientMessage()
        msg.type = ClientMessage.FEATURE_SUBSCRIBE

        feature_info = FeatureInfo()
        feature_info.type = FeatureInfo.UTILITY_MEASURE
        msg.feature_info = feature_info

        response = self._client.send(msg)

        if response.type == ServerResponse.FEATURE_ACKNOWLEDGE:
            return response.feature_ack_info.id

        raise RuntimeError("Could not register UtilityMeasure feature with the server")

    def update_utility(self, new_measure: float):
        self._utility_measures.append(new_measure)

    def clear_utility(self):
        self._utility_measures.clear()
