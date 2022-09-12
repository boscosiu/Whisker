import './App.scss';

import React from 'react';
import Button from 'react-bootstrap/Button';
import Col from 'react-bootstrap/Col';
import Container from 'react-bootstrap/Container';
import Form from 'react-bootstrap/Form';
import Nav from 'react-bootstrap/Nav';
import Navbar from 'react-bootstrap/Navbar';
import Tab from 'react-bootstrap/Tab';
import ToggleButton from 'react-bootstrap/ToggleButton';
import ToggleButtonGroup from 'react-bootstrap/ToggleButtonGroup';
import Connection from './Connection';
import MapControl from './MapControl';
import ServerControl from './ServerControl';
import VehicleControl from './VehicleControl';

class App extends React.Component {
    constructor(props) {
        super(props);

        const messageHandlers = new Map()
            .set('ServerStateMessage', (msg) => {
                const vehicleIds = Object.keys(msg.vehicles);
                this.setState((state) => ({
                    mapIds: msg.mapIds,
                    selectedMapIds: state.selectedMapIds.filter((mapId) => msg.mapIds.includes(mapId)),
                    vehicleIds: vehicleIds,
                    selectedVehicleIds: state.selectedVehicleIds.filter((vehicleId) => vehicleIds.includes(vehicleId)),
                    vehicleDataMap: msg.vehicles
                }));
            })
            .set('ResourceFilesMessage', (msg) => {
                this.setState({mapResourceFiles: msg.maps});
            })
            .set('MapDataMessage', (msg) => {
                const mapControl = this.mapControls.get(msg.mapId);
                if (mapControl) {
                    mapControl.processMapData(msg);
                }
            })
            .set('SubmapTextureMessage', (msg) => {
                const mapControl = this.mapControls.get(msg.mapId);
                if (mapControl) {
                    mapControl.processSubmapTexture(msg);
                }
            })
            .set('VehiclePosesMessage', (msg) => {
                const mapControl = this.mapControls.get(msg.mapId);
                if (mapControl) {
                    mapControl.processVehiclePoses(msg.vehiclePoses);
                }
            });

        this.connection = new Connection(messageHandlers, (isConnected) => this.onConnectionChange(isConnected));
        this.mapControls = new Map();

        this.state = {
            serverAddress: window.location.hostname + ':9001',
            consoleId: 'console0',
            isConnected: false,
            mapIds: [],
            selectedMapIds: [],
            vehicleIds: [],
            selectedVehicleIds: [],
            vehicleDataMap: {},
            mapResourceFiles: []
        };
    }

    onConnectionChange(isConnected) {
        if (isConnected) {
            this.setState({isConnected: true});
        } else {
            this.setState({
                isConnected: false,
                mapIds: [],
                selectedMapIds: [],
                vehicleIds: [],
                selectedVehicleIds: [],
                vehicleDataMap: {},
                mapResourceFiles: []
            });
        }
    }

    render() {
        return (
            <div className='d-flex flex-column vh-100'>
                <Navbar expand='lg' bg='dark' variant='dark' className='py-1'>
                    <Container fluid>
                        <Navbar.Toggle aria-controls='navbar' className='p-0'/>
                        <Navbar.Collapse id='navbar' className='flex-wrap'>
                            <Col className='col-auto'>
                                <Form.Control
                                    title='Server (host:port)'
                                    placeholder='Server (host:port)'
                                    onChange={(e) => this.setState({serverAddress: e.target.value})}
                                    value={this.state.serverAddress}
                                    disabled={this.state.isConnected}/>
                            </Col>
                            <Col className='col-auto'>
                                <Form.Control
                                    title='Console ID'
                                    placeholder='Console ID'
                                    onChange={(e) => this.setState({consoleId: e.target.value})}
                                    value={this.state.consoleId}
                                    disabled={this.state.isConnected}/>
                            </Col>
                            <div>
                                {this.state.isConnected ?
                                    (<Button
                                        variant='danger'
                                        onClick={() => this.connection.close()}>
                                        Disconnect
                                    </Button>) :
                                    (<Button
                                        variant='success'
                                        onClick={() => this.connection.open(this.state.serverAddress, this.state.consoleId)}>
                                        Connect
                                    </Button>)
                                }
                            </div>
                            <div className='vr mx-3 d-none d-lg-block'/>
                            <ToggleButtonGroup
                                type='checkbox'
                                value={this.state.selectedMapIds}
                                onChange={(selected) => this.setState({selectedMapIds: selected})}>
                                {this.state.mapIds.map((mapId) =>
                                    <ToggleButton variant='info' key={mapId} id={mapId} value={mapId}>
                                        {mapId}
                                    </ToggleButton>
                                )}
                            </ToggleButtonGroup>
                            <ToggleButtonGroup
                                type='checkbox'
                                value={this.state.selectedVehicleIds}
                                onChange={(selected) => this.setState({selectedVehicleIds: selected})}>
                                {this.state.vehicleIds.map((vehicleId) =>
                                    <ToggleButton variant='warning' key={vehicleId} id={vehicleId} value={vehicleId}>
                                        {vehicleId}
                                    </ToggleButton>
                                )}
                            </ToggleButtonGroup>
                        </Navbar.Collapse>
                    </Container>
                </Navbar>
                <Tab.Container>
                    <Nav variant='tabs'>
                        {this.state.isConnected &&
                            <Nav.Item>
                                <Nav.Link eventKey='server' role='button'>Server</Nav.Link>
                            </Nav.Item>
                        }
                        {this.state.selectedMapIds.map((mapId) =>
                            <Nav.Item key={mapId}>
                                <Nav.Link eventKey={'m_' + mapId} role='button'>{mapId}</Nav.Link>
                            </Nav.Item>
                        )}
                        {this.state.selectedVehicleIds.map((vehicleId) =>
                            <Nav.Item key={vehicleId}>
                                <Nav.Link eventKey={'v_' + vehicleId} role='button'>{vehicleId}</Nav.Link>
                            </Nav.Item>
                        )}
                    </Nav>
                    <Tab.Content className='flex-grow-1 overflow-hidden'>
                        {this.state.isConnected &&
                            <Tab.Pane eventKey='server' className='overflow-auto h-100'>
                                <ServerControl
                                    connection={this.connection}
                                    mapIds={this.state.mapIds}
                                    mapResourceFiles={this.state.mapResourceFiles}/>
                            </Tab.Pane>
                        }
                        {this.state.selectedMapIds.map((mapId) =>
                            <Tab.Pane key={mapId} eventKey={'m_' + mapId} className='h-100'>
                                <MapControl
                                    ref={(component) => {
                                        if (component) {
                                            this.mapControls.set(mapId, component);
                                        } else {
                                            this.mapControls.delete(mapId);
                                        }
                                    }}
                                    connection={this.connection}
                                    vehicleDataMap={this.state.vehicleDataMap}
                                    mapId={mapId}/>
                            </Tab.Pane>
                        )}
                        {this.state.selectedVehicleIds.map((vehicleId) =>
                            <Tab.Pane key={vehicleId} eventKey={'v_' + vehicleId} className='overflow-auto h-100'>
                                <VehicleControl
                                    connection={this.connection}
                                    mapIds={this.state.mapIds}
                                    vehicleData={this.state.vehicleDataMap[vehicleId]}
                                    vehicleId={vehicleId}/>
                            </Tab.Pane>
                        )}
                    </Tab.Content>
                </Tab.Container>
            </div>
        );
    }
}

export default App;
