import React from 'react';
import Button from 'react-bootstrap/Button';
import Card from 'react-bootstrap/Card';
import Col from 'react-bootstrap/Col';
import Container from 'react-bootstrap/Container';
import Form from 'react-bootstrap/Form';
import InputGroup from 'react-bootstrap/InputGroup';
import Modal from "react-bootstrap/Modal";
import Row from 'react-bootstrap/Row';
import Stack from 'react-bootstrap/Stack';

class VehicleControl extends React.Component {
    constructor(props) {
        super(props);

        this.state = {
            initialPoseX: '0',
            initialPoseY: '0',
            initialPoseR: '0',
            allowGlobalLocalization: false,
            useLocalizationTrimmer: false,
            deleteModalShown: false
        };

        this.assignToMapSelection = React.createRef();
        this.capabilityInputRefs = {};
    }

    assignToMap(mapId) {
        const requestAssignVehicleToMapMessage = this.props.connection.newMessage('RequestAssignVehicleToMapMessage', {
            vehicleId: this.props.vehicleId,
            mapId: mapId
        });
        if (mapId !== '') {
            requestAssignVehicleToMapMessage.initialPose = {
                x: this.state.initialPoseX,
                y: this.state.initialPoseY,
                r: this.state.initialPoseR * Math.PI / 180
            };
            requestAssignVehicleToMapMessage.allowGlobalLocalization = this.state.allowGlobalLocalization;
            requestAssignVehicleToMapMessage.useLocalizationTrimmer = this.state.useLocalizationTrimmer;
        }
        this.props.connection.sendMessage(requestAssignVehicleToMapMessage);
    }

    startObservationLog() {
        this.props.connection.sendNewMessage('RequestStartObservationLogMessage', {vehicleId: this.props.vehicleId});
    }

    stopObservationLog() {
        this.props.connection.sendNewMessage('RequestStopObservationLogMessage', {vehicleId: this.props.vehicleId});
    }

    invokeCapability(capability, input) {
        this.props.connection.sendNewMessage('InvokeCapabilityMessage', {
            vehicleId: this.props.vehicleId,
            capability: capability,
            input: input
        });
    }

    renderSelector(items, ref) {
        return (
            <Form.Select ref={ref}>
                {items.map((item) =>
                    <option key={item}>{item}</option>
                )}
            </Form.Select>
        );
    }

    renderCapabilityControl(capability) {
        if (!this.capabilityInputRefs[capability]) {
            this.capabilityInputRefs[capability] = React.createRef();
        }

        return (
            <InputGroup key={capability}>
                <Button onClick={() =>
                    this.invokeCapability(capability, this.capabilityInputRefs[capability].current.value)}>
                    {capability}
                </Button>
                <Form.Control
                    title='input'
                    placeholder='input'
                    ref={this.capabilityInputRefs[capability]}/>
            </InputGroup>
        );
    }

    renderDeleteModal() {
        const closeModal = () => this.setState({deleteModalShown: false});
        const doDelete = () => {
            this.props.connection.sendNewMessage('RequestDeleteVehicleMessage', {vehicleId: this.props.vehicleId});
            closeModal();
        };

        return (
            <Modal show={this.state.deleteModalShown} onHide={closeModal}>
                <Modal.Header closeButton>
                    <Modal.Title>Confirm Deletion</Modal.Title>
                </Modal.Header>
                <Modal.Body>Delete vehicle '{this.props.vehicleId}'?</Modal.Body>
                <Modal.Footer>
                    <Button variant='danger' onClick={doDelete}>Delete</Button>
                </Modal.Footer>
            </Modal>
        );
    }

    render() {
        return (
            <>
                <Container>
                    <Row xs='auto' className='g-3'>
                        <Col>
                            <Card>
                                <Card.Header>Assigned Map</Card.Header>
                                <Card.Body>{this.props.vehicleData.assignedMapId}</Card.Body>
                            </Card>
                        </Col>
                        <Col>
                            <Card>
                                <Card.Header>Assign To Map</Card.Header>
                                <Card.Body>
                                    <Stack>
                                        {this.renderSelector(this.props.mapIds, this.assignToMapSelection)}
                                        initial pose x (m), y (m), r (°):
                                        <Form.Control
                                            title='x (m)'
                                            placeholder='x (m)'
                                            onChange={(e) => this.setState({initialPoseX: e.target.value})}
                                            value={this.state.initialPoseX}/>
                                        <Form.Control
                                            title='y (m)'
                                            placeholder='y (m)'
                                            onChange={(e) => this.setState({initialPoseY: e.target.value})}
                                            value={this.state.initialPoseY}/>
                                        <Form.Control
                                            title='r (°)'
                                            placeholder='r (°)'
                                            onChange={(e) => this.setState({initialPoseR: e.target.value})}
                                            value={this.state.initialPoseR}/>
                                        <Form.Check
                                            type='checkbox'
                                            label='Allow global localization'
                                            onChange={(e) => this.setState({allowGlobalLocalization: e.target.checked})}
                                            checked={this.state.allowGlobalLocalization}/>
                                        <Form.Check
                                            type='checkbox'
                                            label='Use localization trimmer'
                                            onChange={(e) => this.setState({useLocalizationTrimmer: e.target.checked})}
                                            checked={this.state.useLocalizationTrimmer}/>
                                    </Stack>
                                    <Button onClick={() => this.assignToMap(this.assignToMapSelection.current.value)}>
                                        Submit
                                    </Button>
                                </Card.Body>
                            </Card>
                        </Col>
                        <Col>
                            <Button onClick={() => this.assignToMap('')}>Unassign From Map</Button>
                        </Col>
                        <Col>
                            <Card>
                                <Card.Header>Observation Log</Card.Header>
                                <Card.Body>
                                    <Button onClick={() => this.startObservationLog()}>Start</Button>
                                    <Button onClick={() => this.stopObservationLog()}>Stop</Button>
                                </Card.Body>
                            </Card>
                        </Col>
                        <Col>
                            <Card>
                                <Card.Header>Capabilities</Card.Header>
                                <Card.Body>
                                    <Stack>
                                        {this.props.vehicleData.capabilities.map((capability) => this.renderCapabilityControl(capability))}
                                    </Stack>
                                </Card.Body>
                            </Card>
                        </Col>
                        <Col>
                            <Button variant='danger' onClick={() => this.setState({deleteModalShown: true})}>
                                Delete Vehicle
                            </Button>
                        </Col>
                    </Row>
                </Container>
                {this.renderDeleteModal()}
            </>
        );
    }
}

export default VehicleControl;
