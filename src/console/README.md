## Console Component

`whisker_console`

This is a browser-based component that provides a simple interface to the maps and vehicles on a server.  It is a [React](https://reactjs.org) app that uses the [CRA](https://create-react-app.dev) template.  [Node.js](https://nodejs.org) is required as a build dependency.

### Server Connection

This component communicates with the server through its [WebSocket connection](../server/README.md#websocket) for console messages.  Ensure this is enabled in the server's config.

The build process will produce a console app bundle in the output directory.  If the server is configured to serve this over HTTP then the console can be accessed by browsers at the server's WebSocket port.

The app can be hosted elsewhere as long as the server address and WebSocket port is specified in the console UI.

### Console ID

The console ID is an arbitrary string used to identify this component to the server.  It can be changed in the console UI while disconnected and should be unique among all consoles.

### Development

The CRA template provides a development mode that can be started by running `npm start`.  This serves a live-reloading app locally on port 3000 that makes development more convenient.  Ensure the server's address and WebSocket port is correct in the console UI.

The `whisker_proto_console_js` target makes the message proto files available to console code.  This target is part of the build logic but needs to be run manually when using development mode.  This only needs to be run the first time development mode is used and when a proto file is changed.
