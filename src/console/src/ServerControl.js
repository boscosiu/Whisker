import React from 'react';
import Button from 'react-bootstrap/Button';
import Card from 'react-bootstrap/Card';
import Col from 'react-bootstrap/Col';
import Container from 'react-bootstrap/Container';
import Form from 'react-bootstrap/Form';
import Modal from 'react-bootstrap/Modal';
import Row from 'react-bootstrap/Row';
import Stack from 'react-bootstrap/Stack';

class ServerControl extends React.Component {
    constructor(props) {
        super(props);

        this.state = {
            createMapId: '',
            createMapUseTrimmer: false,
            loadMapId: '',
            loadMapFrozen: true,
            loadMapUseTrimmer: false,
            deleteModalShown: false,
            deleteModalItem: ''
        };

        this.deleteMapSelection = React.createRef();
        this.saveMapSelection = React.createRef();
        this.loadMapFileSelection = React.createRef();
    }

    createMap() {
        this.props.connection.sendNewMessage('RequestCreateMapMessage', {
            mapId: this.state.createMapId,
            useOverlappingTrimmer: this.state.createMapUseTrimmer
        });
    }

    deleteMap() {
        if (this.deleteMapSelection.current.value !== '') {
            this.setState({deleteModalShown: true, deleteModalItem: this.deleteMapSelection.current.value});
        }
    }

    saveMap() {
        this.props.connection.sendNewMessage('RequestSaveMapMessage', {mapId: this.saveMapSelection.current.value});
    }

    requestResourceFiles() {
        this.props.connection.sendNewMessage('RequestResourceFilesMessage');
    }

    loadMap() {
        this.props.connection.sendNewMessage('RequestLoadMapMessage', {
            mapId: this.state.loadMapId,
            mapFileName: this.loadMapFileSelection.current.value,
            isFrozen: this.state.loadMapFrozen,
            useOverlappingTrimmer: this.state.loadMapUseTrimmer
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

    renderDeleteModal() {
        const closeModal = () => this.setState({deleteModalShown: false, deleteModalItem: ''});
        const doDelete = () => {
            this.props.connection.sendNewMessage('RequestDeleteMapMessage', {mapId: this.state.deleteModalItem});
            closeModal();
        };

        return (
            <Modal show={this.state.deleteModalShown} onHide={closeModal}>
                <Modal.Header closeButton>
                    <Modal.Title>Confirm Deletion</Modal.Title>
                </Modal.Header>
                <Modal.Body>Delete map '{this.state.deleteModalItem}'?</Modal.Body>
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
                                <Card.Header>Create Map</Card.Header>
                                <Card.Body>
                                    <Form.Control
                                        title='Map ID'
                                        placeholder='Map ID'
                                        onChange={(e) => this.setState({createMapId: e.target.value})}
                                        value={this.state.createMapId}/>
                                    <Form.Check
                                        type='checkbox'
                                        label='Use overlapping trimmer'
                                        onChange={(e) => this.setState({createMapUseTrimmer: e.target.checked})}
                                        checked={this.state.createMapUseTrimmer}/>
                                    <Button onClick={() => this.createMap()}>Submit</Button>
                                </Card.Body>
                            </Card>
                        </Col>
                        <Col>
                            <Card>
                                <Card.Header>Delete Map</Card.Header>
                                <Card.Body>
                                    {this.renderSelector(this.props.mapIds, this.deleteMapSelection)}
                                    <Button variant='danger' onClick={() => this.deleteMap()}>Submit</Button>
                                </Card.Body>
                            </Card>
                        </Col>
                        <Col>
                            <Card>
                                <Card.Header>Save Map</Card.Header>
                                <Card.Body>
                                    <Stack>
                                        {this.renderSelector(this.props.mapIds, this.saveMapSelection)}
                                        <Form.Text muted>Unassigns all vehicles from map</Form.Text>
                                    </Stack>
                                    <Button onClick={() => this.saveMap()}>Submit</Button>
                                </Card.Body>
                            </Card>
                        </Col>
                        <Col>
                            <Card>
                                <Card.Header>Load Map</Card.Header>
                                <Card.Body>
                                    <Stack>
                                        <Form.Control
                                            title='Map ID'
                                            placeholder='Map ID'
                                            onChange={(e) => this.setState({loadMapId: e.target.value})}
                                            value={this.state.loadMapId}/>
                                        {this.renderSelector(this.props.mapResourceFiles, this.loadMapFileSelection)}
                                        <Form.Text muted>Press ↻ to refresh list</Form.Text>
                                        <Form.Check
                                            type='checkbox'
                                            label='Freeze'
                                            onChange={(e) => this.setState({loadMapFrozen: e.target.checked})}
                                            checked={this.state.loadMapFrozen}/>
                                        <Form.Check
                                            type='checkbox'
                                            label='Use overlapping trimmer'
                                            onChange={(e) => this.setState({loadMapUseTrimmer: e.target.checked})}
                                            checked={this.state.loadMapUseTrimmer}/>
                                    </Stack>
                                    <Button variant='success' onClick={() => this.requestResourceFiles()}>↻</Button>
                                    <Button onClick={() => this.loadMap()}>Submit</Button>
                                </Card.Body>
                            </Card>
                        </Col>
                    </Row>
                </Container>
                {this.renderDeleteModal()}
            </>
        );
    }
}

export default ServerControl;
