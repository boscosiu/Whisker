import React from 'react';
import * as THREE from 'three';
import {MapControls} from 'three/examples/jsm/controls/MapControls';
import {CSS2DObject, CSS2DRenderer} from 'three/examples/jsm/renderers/CSS2DRenderer';
import SubmapTextureData from './SubmapTextureData';

// milliseconds to wait after receiving data before sending another request
const mapDataRequestPeriod = 200;
const vehiclePosesRequestPeriod = 60;

// arbitrary 'height' values for layering
const cameraHeight = 6;
const indicatorHeight = 4;
const gridHeight = 2;
const submapHeight = 0;

const initialCameraZoom = 40;
const indicatorLineColor = '#ff0000';
const indicatorTextColor = indicatorLineColor;

class MapControl extends React.Component {
    constructor(props) {
        super(props);

        this.submaps = new Map();
        this.indicators = new Map();

        this.camera = new THREE.OrthographicCamera(-10, 10, 10, -10);  // arbitrary initial dimensions
        this.camera.up = new THREE.Vector3(0, 0, 1);
        this.camera.lookAt(0, 0, 0);
        this.camera.position.z = cameraHeight;
        this.camera.zoom = initialCameraZoom;

        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x808080);

        const gridHelper = new THREE.GridHelper(1000, 1000, 0x222222, 0x222222);
        gridHelper.rotateX(-Math.PI / 2);
        gridHelper.position.z = gridHeight;
        this.scene.add(gridHelper);

        const indicatorSegments = 20;
        const indicatorTheta = 2 * Math.PI / indicatorSegments;
        const indicatorPoints = [0, 0, 0];
        for (let i = 0; i <= indicatorSegments; ++i) {
            indicatorPoints.push(Math.cos(i * indicatorTheta), Math.sin(i * indicatorTheta), 0);
        }
        this.indicatorObj = new THREE.Line(
            new THREE.BufferGeometry().setAttribute('position', new THREE.Float32BufferAttribute(indicatorPoints, 3)),
            new THREE.LineBasicMaterial({color: indicatorLineColor}));

        this.mapDataRequestMessage = this.props.connection.newMessage('RequestMapDataMessage',
            {mapId: props.mapId, haveVersion: 0});

        this.vehiclePosesRequestMessage = this.props.connection.newMessage('RequestVehiclePosesMessage',
            {mapId: props.mapId});

        this.mountRef = React.createRef();
        this.mountResizeObserver = new ResizeObserver(() => {
            const {clientWidth, clientHeight} = this.mountRef.current;
            const {width, height} = this.glRenderer.domElement;

            if ((clientWidth !== width) || (clientHeight !== height)) {
                this.glRenderer.setSize(clientWidth, clientHeight, false);
                this.cssRenderer.setSize(clientWidth, clientHeight);
                this.camera.left = -clientWidth / 2;
                this.camera.right = clientWidth / 2;
                this.camera.top = clientHeight / 2;
                this.camera.bottom = -clientHeight / 2;
                this.camera.updateProjectionMatrix();
                this.requestRender();
            }
        });
    }

    componentDidMount() {
        this.isPendingRender = false;
        this.isShuttingDown = false;

        this.glRenderer = new THREE.WebGLRenderer();
        this.glRenderer.domElement.style.position = 'absolute';
        this.mountRef.current.appendChild(this.glRenderer.domElement);

        this.cssRenderer = new CSS2DRenderer();
        this.cssRenderer.domElement.style.position = 'absolute';
        this.mountRef.current.appendChild(this.cssRenderer.domElement);

        this.controls = new MapControls(this.camera, this.cssRenderer.domElement);
        this.controls.minPolarAngle = this.controls.maxPolarAngle = 0;
        this.controls.addEventListener('change', () => {
            this.requestRender();
        });

        this.mountResizeObserver.observe(this.mountRef.current);

        this.resetMapDataRequestTimeout();
        this.resetVehiclePosesRequestTimeout();
    }

    componentWillUnmount() {
        this.isShuttingDown = true;
        clearTimeout(this.vehiclePosesRequestTimeout);
        clearTimeout(this.mapDataRequestTimeout);
        this.mountResizeObserver.disconnect();
        this.controls.dispose();
        this.cssRenderer.domElement.remove();
        this.glRenderer.domElement.remove();
        this.glRenderer.dispose();
    }

    requestRender() {
        if (!this.isPendingRender && !this.isShuttingDown) {
            this.isPendingRender = true;
            requestAnimationFrame(() => {
                this.isPendingRender = false;
                this.glRenderer.render(this.scene, this.camera);
                this.cssRenderer.render(this.scene, this.camera);
            });
        }
    }

    resetMapDataRequestTimeout() {
        clearTimeout(this.mapDataRequestTimeout);
        this.mapDataRequestTimeout = setTimeout(() => {
            this.props.connection.sendMessage(this.mapDataRequestMessage);
        }, mapDataRequestPeriod);
    }

    resetVehiclePosesRequestTimeout() {
        clearTimeout(this.vehiclePosesRequestTimeout);
        this.vehiclePosesRequestTimeout = setTimeout(() => {
            this.props.connection.sendMessage(this.vehiclePosesRequestMessage);
        }, vehiclePosesRequestPeriod);
    }

    getSubmapKeyFromProto(protoMsg) {
        return protoMsg.trajectoryId + ':' + protoMsg.index;
    }

    getOrCreateSubmapData(key) {
        let submapData = this.submaps.get(key);
        if (!submapData) {
            submapData = new SubmapTextureData(this.scene, submapHeight);
            this.submaps.set(key, submapData);
        }
        return submapData
    }

    clearSubmaps(keys) {
        for (let key of keys) {
            const submapObject = this.submaps.get(key);
            if (submapObject) {
                submapObject.dispose();
                this.submaps.delete(key);
            }
        }
    }

    processMapData(mapData) {
        if (this.isShuttingDown) {
            return;
        }

        const submapTexturesToRequest = [];

        let obsoleteSubmapKeys;
        if (mapData.isNewMapVersion) {
            obsoleteSubmapKeys = new Set();
            for (let key of this.submaps.keys()) {
                obsoleteSubmapKeys.add(key);
            }
        }

        mapData.submaps.forEach((submap) => {
            const key = this.getSubmapKeyFromProto(submap.submapId);
            const submapData = this.getOrCreateSubmapData(key);

            if (submapData.getVersion() < submap.version) {
                submapTexturesToRequest.push(submap.submapId);
                submapData.setVersion(submap.version);
            }

            if (mapData.isNewMapVersion) {
                submapData.setGlobalPose(submap.globalPose);
                obsoleteSubmapKeys.delete(key);
            }
        });

        if (submapTexturesToRequest.length > 0) {
            this.props.connection.sendNewMessage('RequestSubmapTexturesMessage', {
                mapId: this.props.mapId,
                submapIds: submapTexturesToRequest
            });
        }

        this.resetMapDataRequestTimeout();

        if (mapData.isNewMapVersion) {
            this.mapDataRequestMessage.haveVersion = mapData.mapVersion;
            this.clearSubmaps(obsoleteSubmapKeys);
            this.requestRender();
        }
    }

    processSubmapTexture(submap) {
        if (this.isShuttingDown) {
            return;
        }

        const url = URL.createObjectURL(new Blob([submap.texture], {type: 'image/png'}));

        const image = new Image();
        image.src = url;
        image.decode().then(() => {
            URL.revokeObjectURL(url);

            const submapData = this.getOrCreateSubmapData(this.getSubmapKeyFromProto(submap.submapId));

            if (submapData.getVersion() <= submap.version) {
                submapData.setVersion(submap.version);
                submapData.setTexture(image, submap.resolution);
                submapData.setSubmapPose(submap.submapPose);

                this.requestRender();
            }
        });

        this.resetMapDataRequestTimeout();
    }

    processVehiclePoses(vehiclePoses) {
        if (this.isShuttingDown) {
            return;
        }

        const obsoleteIndicatorKeys = new Set();
        for (let key of this.indicators.keys()) {
            obsoleteIndicatorKeys.add(key);
        }

        vehiclePoses.forEach((vehiclePose) => {
            const vehicleId = vehiclePose.vehicleId;
            const x = vehiclePose.pose.x;
            const y = vehiclePose.pose.y;
            const r = vehiclePose.pose.r;

            obsoleteIndicatorKeys.delete(vehicleId);

            let indicator = this.indicators.get(vehicleId);

            if (!indicator) {
                indicator = this.indicatorObj.clone();
                indicator.scale.setScalar(this.props.vehicleDataMap[vehicleId].keepOutRadius);

                const label = new CSS2DObject();
                label.element.style.color = indicatorTextColor;
                label.element.style.whiteSpace = 'pre';
                label.element.style.lineHeight = '1';
                label.element.style.marginTop = '55px';
                indicator.add(label);

                this.indicators.set(vehicleId, indicator);
                this.scene.add(indicator);
            }

            indicator.children[0].element.textContent =
                `${vehicleId}\n${x.toFixed(1)}\n${y.toFixed(1)}\n${(r * 180 / Math.PI).toFixed()}°`;
            indicator.setRotationFromEuler(new THREE.Euler(0, 0, r));
            indicator.position.set(x, y, indicatorHeight);

            this.requestRender();
        });

        for (let key of obsoleteIndicatorKeys) {
            const indicator = this.indicators.get(key);
            indicator.clear();  // to trigger cleanup of the CSS2D label
            indicator.removeFromParent();
            this.indicators.delete(key);
            this.requestRender();
        }

        this.resetVehiclePosesRequestTimeout();
    }

    render() {
        return (
            <div ref={this.mountRef} className='h-100'/>
        );
    }
}

export default MapControl;
