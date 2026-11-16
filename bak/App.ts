
import { Server } from '@overnightjs/core';
import {
  Request,
  Response,
  NextFunction,
  static as expressStatic
} from 'express';

import bodyParser from 'body-parser';
import path from 'path';
import request from 'superagent';

import {
  Program,
  Api,
  Notify,
  UnrealEngine,
  LogServer
} from './';


export class App extends Server {

  // 서버가 살아 있는 동안 유지되는 현재 Review 상태
  private activeReviewId: number = -1;

  // 같은 ID를 다시 LOAD해도 변경 이벤트로 인식
  private activeReviewVersion: number = 0;


  constructor() {
    super();

    this.app.use(bodyParser.json());

    this.app.use(
      bodyParser.urlencoded({ extended: false })
    );

    this.app.use(
      (req, res, next) => App.setAccessControl(res, next)
    );

    this.app.use(
      expressStatic(
        path.join(Program.rootFolder, 'public'),
        {
          setHeaders: (res) => App.setAccessControl(res)
        }
      )
    );


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

    this.app.post(
      '/vpsync/review-drawing',
      async (req: Request, res: Response) => {

        try {

          const drawingData = req.body.DrawingData;

          if (
            typeof drawingData !== 'string' ||
            !drawingData.trim()
          ) {

            return res.status(400).send({
              error: 'Invalid DrawingData'
            });

          }

          const rcUrl =
            'http://127.0.0.1:30010/remote/object/call';


          // 1. 현재 실행 중인 PIE World 조회

          const worldResponse = await request
            .put(rcUrl)
            .send({
              objectPath:
                '/Script/EditorScriptingUtilities.Default__EditorLevelLibrary',

              functionName: 'GetPIEWorlds',

              parameters: {
                bIncludeDedicatedServer: false
              },

              generateTransaction: false
            });

          const pieWorlds = worldResponse.body.ReturnValue;

          if (
            !Array.isArray(pieWorlds) ||
            pieWorlds.length !== 1
          ) {

            return res.status(409).send({
              error: 'Expected exactly one PIE World'
            });

          }


          // 2. PIE World의 Controller 검색

          const actorResponse = await request
            .put(rcUrl)
            .send({
              objectPath:
                '/Script/Engine.Default__GameplayStatics',

              functionName: 'GetAllActorsOfClass',

              parameters: {
                WorldContextObject: pieWorlds[0],

                ActorClass:
                  '/Game/Junha_Made/Blueprints/Controller/BP_VPSessionController.BP_VPSessionController_C'
              },

              generateTransaction: false
            });

          const controllers = actorResponse.body.OutActors;

          if (
            !Array.isArray(controllers) ||
            controllers.length !== 1
          ) {

            return res.status(409).send({
              error: 'Expected exactly one PIE Controller'
            });

          }


          // 3. PIE Controller에 필기 데이터 전달

          await request
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

        } catch (error) {

          console.error(
            'Review drawing error:',
            error
          );

          return res.status(500).send({
            error: 'Review drawing delivery failed'
          });

        }

      }
    );


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

    this.app.get(
      '/vpsync/reviews/:id/image',
      async (req: Request, res: Response) => {

        try {

          // 1. Review ID 읽기

          const reviewId = Number(req.params.id);


          // 2. 유효한 Review ID인지 검사

          if (
            !Number.isSafeInteger(reviewId) ||
            reviewId <= 0
          ) {

            return res.status(400).send({
              error: 'Invalid Review ID'
            });

          }


          // 3. ASP.NET Core 서버 주소
          //
          // 현재 환경에서 사용하는 API 포트 확인 필요

          const reviewApiBaseUrl =
            'http://127.0.0.1:5000';


          // 4. 저장된 PNG 조회 주소 구성

          const imageUrl =
            `${reviewApiBaseUrl}/api/Reviews/${reviewId}/image`;


          // 5. ASP.NET Core API에 GET 요청
          //
          // PNG 바이너리를 Buffer로 받음

          const imageResponse = await request
            .get(imageUrl)
            .responseType('blob')
            .timeout({
              response: 5000,
              deadline: 10000
            });


          // 6. 바이너리 데이터 확인

          const imageBuffer = imageResponse.body;

          if (
            !Buffer.isBuffer(imageBuffer) ||
            imageBuffer.length === 0
          ) {

            console.error(
              'Invalid image response:',
              reviewId
            );

            return res.status(502).send({
              error: 'Invalid image data from Review API'
            });

          }


          // 7. PNG 응답 헤더 설정

          res.setHeader(
            'Content-Type',
            'image/png'
          );

          res.setHeader(
            'Cache-Control',
            'no-store'
          );


          // 8. PNG 바이너리를 클라이언트에 반환

          return res.status(200).end(imageBuffer);

        } catch (error: any) {

          console.error(
            'Review image proxy error:',
            error
          );


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

      }
      );


      // =====================================================
      // 08-137: Active Review State
      //
      // LIVE    : reviewId = -1
      // ARCHIVE : reviewId > 0
      // =====================================================

      this.app.post(
          '/vpsync/active-review',
          (req: Request, res: Response) => {

              const mode = req.body?.mode;
              const reviewId = Number(req.body?.reviewId);

              if (mode !== 'LIVE' && mode !== 'ARCHIVE') {
                  return res.status(400).json({
                      error: 'Invalid mode'
                  });
              }

              if (
                  mode === 'ARCHIVE' &&
                  (!Number.isSafeInteger(reviewId) || reviewId <= 0)
              ) {
                  return res.status(400).json({
                      error: 'Invalid Review ID'
                  });
              }

              this.activeReviewId =
                  mode === 'LIVE' ? -1 : reviewId;

              // 동일 Review ID를 다시 LOAD했을 때도 갱신
              this.activeReviewVersion += 1;

              res.setHeader('Cache-Control', 'no-store');

              return res.json({
                  ok: true,
                  mode: this.activeReviewId < 0
                      ? 'LIVE'
                      : 'ARCHIVE',
                  reviewId: this.activeReviewId,
                  version: this.activeReviewVersion
              });
          }
      );

      this.app.get(
          '/vpsync/active-review',
          (req: Request, res: Response) => {

              res.setHeader('Cache-Control', 'no-store');

              return res.json({
                  mode: this.activeReviewId < 0
                      ? 'LIVE'
                      : 'ARCHIVE',
                  reviewId: this.activeReviewId,
                  version: this.activeReviewVersion
              });
          }
      );

      // 저장된 Review 메타데이터 중계
      // iPad가 ASP.NET 서버에 직접 접근할 필요가 없도록 한다.

      this.app.get(
          '/vpsync/reviews/:id',
          async (req: Request, res: Response) => {

              const id = Number(req.params.id);

              if (!Number.isSafeInteger(id) || id <= 0) {
                  return res.status(400).json({
                      error: 'Invalid Review ID'
                  });
              }

              try {
                  const apiResponse = await request
                      .get(`http://127.0.0.1:5000/api/reviews/${id}`)
                      .accept('application/json')
                      .timeout({
                          response: 5000,
                          deadline: 10000
                      });

                  res.setHeader('Cache-Control', 'no-store');

                  return res.json(apiResponse.body);

              } catch (error: any) {

                  if (error && error.status === 404) {
                      return res.status(404).json({
                          error: 'Review not found'
                      });
                  }

                  console.error(
                      'Review metadata proxy failed:',
                      error
                  );

                  return res.status(502).json({
                      error: 'Review metadata retrieval failed'
                  });
              }
          }
      );


  }


  // =====================================================
  // WebApp 시작
  // 기존 기능 유지
  // =====================================================

  public async start(): Promise<void> {

    try {

      const api = new Api();

      api.initialize();

      super.addControllers([api]);

      this.app.use(
        '/api',
        (req, res) => App.notFoundHandler(req, res, true)
      );


      // Trying to kill a zombie process

      await request
        .get(
          `http://127.0.0.1:${Program.port}/api/shutdown`
        )
        .timeout(100)
        .then(
          () => new Promise(
            resolve => setTimeout(resolve, 100)
          )
        )
        .catch(() => {});


      await this.startServer();

      var hostname = Program.hostname;

      if (hostname == '0.0.0.0') {
        hostname = '127.0.0.1';
      }

      console.log(
        `DONE: WebApp started, ${hostname}:${Program.port}`
      );

    } catch (error) {

      console.error('ERROR:', error.message);

      throw error;

    }

  }


  // =====================================================
  // HTTP 서버 실행
  // 기존 기능 유지
  // =====================================================

  private startServer(): Promise<void> {

    return new Promise((resolve, reject) => {

      const server = this.app.listen(
        Program.port,
        Program.hostname,
        async () => {

          await Notify.initialize(server);

          await UnrealEngine.initialize();

          await LogServer.initialize();

          resolve();

        }
      );

      server.once('error', reject);

    });

  }


  // =====================================================
  // CORS 헤더
  // 기존 기능 유지
  // =====================================================

  private static setAccessControl(
    res: Response,
    next?: NextFunction
  ): void {

    res.header('X-Powered-By', 'Conductor');

    res.header('Access-Control-Allow-Origin', '*');

    res.header('Access-Control-Allow-Headers', '*');

    res.header('Access-Control-Max-Age', '86400');

    res.header(
      'Access-Control-Allow-Methods',
      'GET, POST, PUT, DELETE, OPTIONS'
    );

    next?.();

  }


  // =====================================================
  // 404 처리
  // 기존 기능 유지
  // =====================================================

  private static notFoundHandler(
    req: Request,
    res: Response,
    isApi: boolean
  ): void {

    res.status(404).send({
      error:
        `Invalid ${isApi ? 'api call' : 'request'}: ${req.method} '${req.originalUrl}'`
    });

  }

}
