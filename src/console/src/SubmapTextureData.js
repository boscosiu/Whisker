import * as THREE from 'three';

const vertexShader = `
    varying vec2 vUv;

    void main() {
        vUv = uv;
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
    }
`;
const fragmentShader = `
    uniform bool shadePositiveLogOdds;
    uniform sampler2D submapTexture;
    varying vec2 vUv;

    void main() {
        vec4 texel = texture2D(submapTexture, vUv);
        float logOdds = texel.r;
        if (logOdds == 0.0) {
            discard;
        } else {
            logOdds -= 0.5;
            if (shadePositiveLogOdds) {
                if (logOdds > 0.0) {
                    gl_FragColor = vec4(logOdds, logOdds, logOdds, 0.0);
                } else {
                    discard;
                }
            } else {
                if (logOdds < 0.0) {
                    gl_FragColor = vec4(-logOdds, -logOdds, -logOdds, 0.0);
                } else {
                    discard;
                }
            }
        }
    }
`;
const geometry = new THREE.PlaneGeometry(1, 1);

class SubmapTextureData {
    constructor(scene, submapHeight) {
        this.version = 0;
        this.scene = scene;
        this.addedToScene = false;
        this.submapHeight = submapHeight;
        this.global_pose = new THREE.Object3D();
        this.submap_pose = new THREE.Object3D();
        this.texture = null;
        this.materialAdd = null;
        this.materialSub = null;

        this.global_pose.add(this.submap_pose);
    }

    dispose() {
        this.global_pose.removeFromParent();
        if (this.texture) {
            this.materialSub.dispose();
            this.materialAdd.dispose();
            this.texture.dispose();
        }
    }

    getVersion() {
        return this.version;
    }

    setVersion(version) {
        this.version = version;
    }

    setGlobalPose(transformProto) {
        if (!this.addedToScene) {
            this.scene.add(this.global_pose);
            this.addedToScene = true;
        }
        this.global_pose.setRotationFromEuler(new THREE.Euler(0, 0, transformProto.r));
        this.global_pose.position.set(transformProto.x, transformProto.y, this.submapHeight);
    }

    setSubmapPose(transformProto) {
        this.submap_pose.setRotationFromEuler(new THREE.Euler(0, 0, transformProto.r));
        this.submap_pose.position.set(transformProto.x, transformProto.y, 0);
    }

    setTexture(image, resolution) {
        const texture = new THREE.Texture(image);
        texture.generateMipmaps = false;
        texture.magFilter = texture.minFilter = THREE.NearestFilter;
        texture.format = THREE.RedFormat;
        texture.needsUpdate = true;

        if (this.texture) {
            this.texture.dispose();
            this.texture = texture;

            this.materialAdd.uniforms.submapTexture.value = texture;
            this.materialSub.uniforms.submapTexture.value = texture;
        } else {
            this.texture = texture;

            const commonMaterialProperties = {
                vertexShader: vertexShader,
                fragmentShader: fragmentShader,
                blending: THREE.CustomBlending,
                blendSrc: THREE.OneFactor,
                blendDst: THREE.OneFactor,
                depthWrite: false
            };
            this.materialAdd = new THREE.ShaderMaterial({
                ...commonMaterialProperties,
                ...{
                    uniforms: {shadePositiveLogOdds: {value: true}, submapTexture: {value: texture}},
                    blendEquation: THREE.AddEquation
                }
            });
            this.materialSub = new THREE.ShaderMaterial({
                ...commonMaterialProperties,
                ...{
                    uniforms: {shadePositiveLogOdds: {value: false}, submapTexture: {value: texture}},
                    blendEquation: THREE.ReverseSubtractEquation
                }
            });

            this.submap_pose.add(new THREE.Mesh(geometry, this.materialAdd));
            this.submap_pose.add(new THREE.Mesh(geometry, this.materialSub));
        }

        this.submap_pose.scale.set(texture.image.width * resolution, texture.image.height * resolution, 1);
    }
}

export default SubmapTextureData;
