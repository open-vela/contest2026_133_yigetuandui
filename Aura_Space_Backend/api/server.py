from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
import uvicorn

app = FastAPI(title="Aura‑Space Backend API")

# 修复CORS：禁止 allow_origins=["*"] + allow_credentials=True共存
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://127.0.0.1:4173",
        "http://localhost:4173"
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/api/dashboard")
@app.get("/dashboard")
async def get_dashboard():
    return {
        "radar": {
            "reaction": 80,
            "focus": 75,
            "memory": 90,
            "pressure": 60,
            "flow": 85
        },
        "recommendation": "系统状态极佳，建议立刻开始 BeatFlicks 训练！"
    }

@app.get("/api/beatmap")
@app.get("/beatmap")
async def get_beatmap():
    return {
        "bpm": 120,
        "notes": [
            {"timestamp_ms": 1000, "type": 0, "track_id": 1},
            {"timestamp_ms": 2000, "type": 1, "track_id": 1},
            {"timestamp_ms": 3000, "type": 2, "track_id": 1}
        ]
    }

@app.post("/api/game_result")
@app.post("/game_result")
async def receive_game_result(result: dict):
    print(f"收到前端传来的游戏结算数据: {result}")
    return {"status": "success", "message": "结算数据已接收"}


if __name__ == "__main__":
    print("启动 Aura‑Space 本地后端服务...")
    uvicorn.run(app, host="0.0.0.0", port=8000)
