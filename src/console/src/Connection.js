import {load as loadProtobuf} from 'protobufjs';

const protoNamespace = 'whisker.proto.';

class Connection {
    constructor(messageHandlers, connectionChangeHandler) {
        this.websocket = null;
        this.messageHandlers = new Map();
        this.connectionChangeHandler = connectionChangeHandler;
        this.protoRoot = null;

        this.loadProtobufPromise = loadProtobuf('console.proto').then((root) => {
            this.protoRoot = root;
            messageHandlers.forEach((handler, messageName) => {
                this.messageHandlers.set(protoNamespace + messageName, {
                    handler: handler,
                    messageType: root.lookupType(protoNamespace + messageName)
                });
            });
        });
    }

    async open(serverAddress, clientId) {
        await this.loadProtobufPromise;

        this.websocket = new WebSocket(`ws://${serverAddress}/?client_id=${clientId}`, 'whisker');
        this.websocket.binaryType = 'arraybuffer';

        this.websocket.onopen = () => {
            this.connectionChangeHandler(true);
        };

        this.websocket.onclose = () => {
            this.websocket = null;
            this.connectionChangeHandler(false);
        };

        this.websocket.onerror = (event) => {
            console.error('WebSocket error', event);
        };

        this.websocket.onmessage = (msg) => {
            const delimiter = new Uint8Array(msg.data).indexOf(0);
            if (delimiter !== -1) {
                const messageName = new TextDecoder().decode(new Uint8Array(msg.data, 0, delimiter));
                const messageHandler = this.messageHandlers.get(messageName);
                if (messageHandler) {
                    messageHandler.handler(messageHandler.messageType.decode(new Uint8Array(msg.data, delimiter + 1)));
                }
            }
        };
    }

    close() {
        if (this.websocket) {
            this.websocket.close();
        }
    }

    newMessage(messageName, properties) {
        return this.protoRoot.lookupType(protoNamespace + messageName).create(properties);
    }

    sendMessage(message) {
        if (this.websocket && (this.websocket.readyState === WebSocket.OPEN)) {
            const messageName = new TextEncoder().encode(protoNamespace + message.$type.name + '\0');
            const serializedMessage = message.constructor.encode(message).finish();
            const payload = new Uint8Array(messageName.length + serializedMessage.length);
            payload.set(messageName, 0);
            payload.set(serializedMessage, messageName.length);
            this.websocket.send(payload);
        }
    }

    sendNewMessage(messageName, properties) {
        this.sendMessage(this.newMessage(messageName, properties));
    }
}

export default Connection;
