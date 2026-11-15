"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.App = void 0;
const core_1 = require("@overnightjs/core");
const express_1 = require("express");
const body_parser_1 = __importDefault(require("body-parser"));
const path_1 = __importDefault(require("path"));
const superagent_1 = __importDefault(require("superagent"));
const _1 = require("./");
class App extends core_1.Server {
    constructor() {
        super();
        this.app.use(body_parser_1.default.json());
        this.app.use(body_parser_1.default.urlencoded({ extended: false }));
        this.app.use((req, res, next) => App.setAccessControl(res, next));
        this.app.use(express_1.static(path_1.default.join(_1.Program.rootFolder, 'public'), {
            setHeaders: (res) => App.setAccessControl(res)
        }));
        // =====================================================
        // VPSync - Review Drawing
        //
        // POST /vpsync/review-drawing
        //
        // ReviewPad 필기 데이터를
        // PIE World의 BP_VPSessionController에 전달
        //
        // 기존 기능 유지
        // =====================================================
        this.app.post('/vpsync/review-drawing', async (req, res) => {
            try {
                const drawingData = req.body.DrawingData;
                if (typeof drawingData !== 'string' ||
                    !drawingData.trim()) {
                    return res.status(400).send({
                        error: 'Invalid DrawingData'
                    });
                }
                const rcUrl = 'http://127.0.0.1:30010/remote/object/call';
                // 1. 현재 실행 중인 PIE World 조회
                const worldResponse = await superagent_1.default
                    .put(rcUrl)
                    .send({
                    objectPath: '/Script/EditorScriptingUtilities.Default__EditorLevelLibrary',
                    functionName: 'GetPIEWorlds',
                    parameters: {
                        bIncludeDedicatedServer: false
                    },
                    generateTransaction: false
                });
                const pieWorlds = worldResponse.body.ReturnValue;
                if (!Array.isArray(pieWorlds) ||
                    pieWorlds.length !== 1) {
                    return res.status(409).send({
                        error: 'Expected exactly one PIE World'
                    });
                }
                // 2. PIE World의 Controller 검색
                const actorResponse = await superagent_1.default
                    .put(rcUrl)
                    .send({
                    objectPath: '/Script/Engine.Default__GameplayStatics',
                    functionName: 'GetAllActorsOfClass',
                    parameters: {
                        WorldContextObject: pieWorlds[0],
                        ActorClass: '/Game/Junha_Made/Blueprints/Controller/BP_VPSessionController.BP_VPSessionController_C'
                    },
                    generateTransaction: false
                });
                const controllers = actorResponse.body.OutActors;
                if (!Array.isArray(controllers) ||
                    controllers.length !== 1) {
                    return res.status(409).send({
                        error: 'Expected exactly one PIE Controller'
                    });
                }
                // 3. PIE Controller에 필기 데이터 전달
                await superagent_1.default
                    .put(rcUrl)
                    .send({
                    objectPath: controllers[0],
                    functionName: 'ReceiveReviewDrawing',
                    parameters: {
                        DrawingData: drawingData
                    },
                    generateTransaction: false
                });
                return res.send({
                    ok: true
                });
            }
            catch (error) {
                console.error('Review drawing error:', error);
                return res.status(500).send({
                    error: 'Review drawing delivery failed'
                });
            }
        });
        // =====================================================
        // [08-130] VPSync - Saved Review Image
        //
        // GET /vpsync/reviews/:id/image
        //
        // WebApp
        //   ↓
        // ASP.NET Core Review API
        //   ↓
        // 저장된 PNG
        //   ↓
        // WebApp을 통해 클라이언트에 반환
        //
        // 예:
        // GET /vpsync/reviews/8/image
        // =====================================================
        this.app.get('/vpsync/reviews/:id/image', async (req, res) => {
            try {
                // 1. Review ID 읽기
                const reviewId = Number(req.params.id);
                // 2. 유효한 Review ID인지 검사
                if (!Number.isSafeInteger(reviewId) ||
                    reviewId <= 0) {
                    return res.status(400).send({
                        error: 'Invalid Review ID'
                    });
                }
                // 3. ASP.NET Core 서버 주소
                //
                // 현재 환경에서 사용하는 API 포트 확인 필요
                const reviewApiBaseUrl = 'http://127.0.0.1:5000';
                // 4. 저장된 PNG 조회 주소 구성
                const imageUrl = `${reviewApiBaseUrl}/api/Reviews/${reviewId}/image`;
                // 5. ASP.NET Core API에 GET 요청
                //
                // PNG 바이너리를 Buffer로 받음
                const imageResponse = await superagent_1.default
                    .get(imageUrl)
                    .responseType('blob')
                    .timeout({
                    response: 5000,
                    deadline: 10000
                });
                // 6. 바이너리 데이터 확인
                const imageBuffer = imageResponse.body;
                if (!Buffer.isBuffer(imageBuffer) ||
                    imageBuffer.length === 0) {
                    console.error('Invalid image response:', reviewId);
                    return res.status(502).send({
                        error: 'Invalid image data from Review API'
                    });
                }
                // 7. PNG 응답 헤더 설정
                res.setHeader('Content-Type', 'image/png');
                res.setHeader('Cache-Control', 'no-store');
                // 8. PNG 바이너리를 클라이언트에 반환
                return res.status(200).end(imageBuffer);
            }
            catch (error) {
                console.error('Review image proxy error:', error);
                // 저장된 이미지가 없으면 404 유지
                if (error && error.status === 404) {
                    return res.status(404).send({
                        error: 'Review image not found'
                    });
                }
                // ASP.NET 서버 장애 또는 통신 오류
                return res.status(502).send({
                    error: 'Review image retrieval failed'
                });
            }
        });
    }
    // =====================================================
    // WebApp 시작
    // 기존 기능 유지
    // =====================================================
    async start() {
        try {
            const api = new _1.Api();
            api.initialize();
            super.addControllers([api]);
            this.app.use('/api', (req, res) => App.notFoundHandler(req, res, true));
            // Trying to kill a zombie process
            await superagent_1.default
                .get(`http://127.0.0.1:${_1.Program.port}/api/shutdown`)
                .timeout(100)
                .then(() => new Promise(resolve => setTimeout(resolve, 100)))
                .catch(() => { });
            await this.startServer();
            var hostname = _1.Program.hostname;
            if (hostname == '0.0.0.0') {
                hostname = '127.0.0.1';
            }
            console.log(`DONE: WebApp started, ${hostname}:${_1.Program.port}`);
        }
        catch (error) {
            console.error('ERROR:', error.message);
            throw error;
        }
    }
    // =====================================================
    // HTTP 서버 실행
    // 기존 기능 유지
    // =====================================================
    startServer() {
        return new Promise((resolve, reject) => {
            const server = this.app.listen(_1.Program.port, _1.Program.hostname, async () => {
                await _1.Notify.initialize(server);
                await _1.UnrealEngine.initialize();
                await _1.LogServer.initialize();
                resolve();
            });
            server.once('error', reject);
        });
    }
    // =====================================================
    // CORS 헤더
    // 기존 기능 유지
    // =====================================================
    static setAccessControl(res, next) {
        res.header('X-Powered-By', 'Conductor');
        res.header('Access-Control-Allow-Origin', '*');
        res.header('Access-Control-Allow-Headers', '*');
        res.header('Access-Control-Max-Age', '86400');
        res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
        next === null || next === void 0 ? void 0 : next();
    }
    // =====================================================
    // 404 처리
    // 기존 기능 유지
    // =====================================================
    static notFoundHandler(req, res, isApi) {
        res.status(404).send({
            error: `Invalid ${isApi ? 'api call' : 'request'}: ${req.method} '${req.originalUrl}'`
        });
    }
}
exports.App = App;
