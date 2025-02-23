#include <Siv3D.hpp>

//フレームレート設定
class FrameRateLimitAddon : public IAddon
{
private:
	static constexpr std::chrono::steady_clock::duration MaxDrift = 10ms;
	std::chrono::steady_clock::duration m_oneFrameDuration;
	std::chrono::time_point<std::chrono::steady_clock> m_sleepUntil;

	// 1フレームあたりの時間を計算
	static std::chrono::steady_clock::duration FPSToOneFrameDuration(int32 targetFPS)
	{
		if (targetFPS <= 0)
		{
			throw Error{ U"不正なフレームレート: {}"_fmt(targetFPS) };
		}
		return std::chrono::duration_cast<std::chrono::steady_clock::duration>(1s) / targetFPS;
	}

public:
	explicit FrameRateLimitAddon(int32 targetFPS)
		: m_oneFrameDuration(FPSToOneFrameDuration(targetFPS))
		, m_sleepUntil(std::chrono::steady_clock::now())
	{
	}

	// 毎フレームの描画反映後に呼ばれる
	virtual void postPresent() override
	{
		m_sleepUntil += m_oneFrameDuration;
		m_sleepUntil = Max(m_sleepUntil, std::chrono::steady_clock::now() - MaxDrift);

		std::this_thread::sleep_until(m_sleepUntil);
	}

	// フレームレートを変更
	void setTargetFPS(int32 targetFPS)
	{
		m_oneFrameDuration = FPSToOneFrameDuration(targetFPS);
	}
};


//識別子定義
typedef struct {
	String shape;//図形の種類
	Vec2 Pos;//図形の位置
}Shape_Pos;

typedef struct {
	RectF rect;
	Triangle triangle;
	Ellipse ellipse;
}Shape;

typedef struct {
	Vec2 Pos;//ボールの位置
	Vec2 move;//ボールの移動
}Ball;


//基本変数定義
Array<Shape_Pos>block;//blockの位置情報

Array<Line>trampoline;//トランポリンの情報
Vec2 trampoline_start{ 0,0 };

int score(0);//gamescore
double ball_speed(200);//ボールの速度
double block_speed(25);

Array<int>erase_index;//配列の削除候補を記録

Ball ball;//ボールの位置情報

Timer block_addtimer{ 3s };//ブロックを3秒起きに配置するタイマー

Array<int>ranking;


//処理
Shape make_shape(Shape_Pos a/*図形の位置情報から図形の定義*/) {
	Shape ret;
	ret.rect = { 1,1,1,1 };
	ret.triangle = { 1,1,1,1 };
	ret.ellipse = { 1,1,1,1 };

	if (a.shape == U"rect")ret.rect = {a.Pos+Vec2 {10,10},80,30};
	else if (a.shape == U"triangle")ret.triangle = { a.Pos + Vec2{10,10},a.Pos + Vec2{90,10},a.Pos + Vec2{50,40} };
	else if(a.shape == U"ellipse") ret.ellipse = { a.Pos+Vec2{50,25},40,15};

	return ret;
}

void shape_initialization(/*図形情報の初期化*/) {
	block.clear();

	for (int i(0); i < 5; i++) {
		Shape_Pos a;
		a.shape = U"rect";
		a.Pos = { i * 100 + 300,0 };

		block.push_back(a);
	}
}

void trampoline_(Circle ball_det/*トランポリンの描画と定義*/) {
		if(trampoline_start!=Vec2{0,0})Line{trampoline_start,Cursor::Pos()}.draw(Palette::Gray);

		if (MouseL.down())trampoline_start = Cursor::Pos();

		if (MouseL.up()) {
			if (not ball_det.intersectsAt(Line{ trampoline_start,Cursor::Pos()})) {
				trampoline.push_back(Line{ trampoline_start,Cursor::Pos() });
				trampoline_start = { 0,0 };
			}
			else trampoline_start = { 0,0 };
		}

		for (auto a : trampoline)a.draw(2,Palette::Gold);
}

Ball ball_reflection(Circle ball_det,auto points/*ボールの反射処理*/) {
	Vec2 contactpoint{ 0,0 };

	if (1/*交点ベクトルの計算*/) {
		int count(0);
		for (auto& point : *points) {
			Circle{ point,4 }.draw(Palette::Gray);
			contactpoint += point / 2.0;
			count++;
		}
		if (count == 1)contactpoint *= 2;
		contactpoint -= ball_det.center;
	}

	if (1/*boll.moveの計算*/) {
		double cos_theta{ (contactpoint.x * ball.move.x + contactpoint.y * ball.move.y) / 20.0 };
		ball.move = { ball.move - 2 * contactpoint * cos_theta / 20.0 };
	}
	ball.Pos += 2 * ball.move * Scene::DeltaTime() * ball_speed;
	return ball;
}

void ball_collision_object(Circle ball_det/*ボールのブロックとトランポリンの衝突処理*/) {
	for (int index(-1); auto a : block) {//ブロックとボールの当たり判定
		index++;

		Shape shape = make_shape(a);

		if (not (shape.rect.intersects(ball_det) or shape.triangle.intersects(ball_det) or shape.ellipse.intersects(ball_det))) {

			shape.rect.draw(Color{255,20,20,128});
			shape.triangle.draw(Color{20,255,20,128});
			shape.ellipse.draw(Color{20,20,255,128});

			continue;
		}

		//当たり処理
		score += 10;//四角形は10点、三角形は30点、楕円形は20点

		auto points = ball_det.intersectsAt(shape.rect);
		if (not points)points = ball_det.intersectsAt(shape.triangle),score+=20;
		if (not points)points = ball_det.intersectsAt(shape.ellipse),score+=10;

		erase_index.push_back(index);

		ball = ball_reflection(ball_det, points);
	}
	for (auto a : erase_index)block.erase(block.begin() + a);
	erase_index.clear();

	for (int index(-1); auto a : trampoline) {//トランポリンとボールの当たり判定
		index++;

		if (auto points = ball_det.intersectsAt(a)) {
			erase_index.push_back(index);

			ball = ball_reflection(ball_det, points);
		}
	}
	for (auto a : erase_index)trampoline.erase(trampoline.begin() + a);
	erase_index.clear();
}

void reset(/*ゲームの初期化*/) {
	block.clear();
	trampoline.clear();
	trampoline_start={ 0,0 };

	score = 0;

	shape_initialization();//ブロックの初期位置

	ball.Pos = { 400,400 };//ボールの初期化
	ball.move = { Cos(45_deg),Sin(45_deg) };

	block_addtimer.restart();
}

void game_over(/*画面描画のみのループ処理*/) {
	Font font(30);//基本文字

	ranking.push_back(score);//ゲームスコアをランキングに反映してソート(降順)
	sort(ranking.rbegin(), ranking.rend());

	while (System::Update()) {
		//ブロック描画
		for (auto a : block) {
			Shape shape = make_shape(a);

			shape.rect.draw(Color{ 255,20,20,128 });
			shape.triangle.draw(Color{ 20,255,20,128 });
			shape.ellipse.draw(Color{ 20,20,255,128 });
		}

		//画面描画
		Rect{ 0,0,300,1000 }.draw();

		font(U"game over").draw(100, 400, Palette::Red);

		ClearPrint();
		Print <<U"score:" << score;
		for (int i(0); i < ranking.size(); i++) {//ランキングを順位をつけ表示
			Print << i + 1 << U"：" << ranking[i];
		}

		//ボール描画
		Circle ball_det{ ball.Pos,20 };
		ball_det.draw(Palette::Orange);

		//壁描画
		Line{ 300,0,300,800 }.draw(2, Palette::Skyblue);
		Line{ 300,0,800,0 }.draw(2, Palette::Skyblue);
		Line{ 800,0,800,800 }.draw(2, Palette::Skyblue);

		Line{ 300,795,800,795 }.draw(2,Palette::Red);

		//トランポリンの描画
		for (auto a : trampoline) {
			a.draw(2,Palette::Gold);
		}

		//リスタート処理
		font(U"スペース押すと時を戻せる").draw(370, 430, Palette::White);

		if (KeySpace.down() or MouseR.down()) {
			reset();
			return;
		}
	}
}


//メインループ
void Main(){
	Window::Resize(800, 800);

	// VSyncを無効化
	Graphics::SetVSyncEnabled(false);

	// FrameRateLimitアドオンを登録
	constexpr int32 targetFPS = 180;
	Addon::Register(U"FrameRateLimit", std::make_unique<FrameRateLimitAddon>(targetFPS));

	shape_initialization();//blockの初期配置

	ball.Pos = { 400,400 };
	ball.move = { Cos(45_deg),Sin(45_deg) };

	double time(0);

	block_addtimer.start();//ブロック配置間隔

	Font font(30);

	while (System::Update()) {
		Rect{ 0,0,300,1000 }.draw();

		//ボール描画
		Circle ball_det{ ball.Pos,20 };
		ball_det.draw(Palette::Orange);

		//ブロック描画
		for (auto a : block) {
			Shape shape = make_shape(a);

			shape.rect.draw(Color{ 255,20,20,128 });
			shape.triangle.draw(Color{ 20,255,20,128 });
			shape.ellipse.draw(Color{ 20,20,255,128 });
		}

		//壁描画
		Line{ 300,0,300,800 }.draw(2, Palette::Skyblue);
		Line{ 300,0,800,0 }.draw(2, Palette::Skyblue);
		Line{ 800,0,800,800 }.draw(2, Palette::Skyblue);

		Line{ 300,795,800,795 }.draw(2, Palette::Red);


		//スタート処理
		font(U"スペース押すと始まるよ").draw(370, 430, Palette::White);

		if (KeySpace.down() or MouseR.down()) {
			break;
		}
	}

	while (System::Update()) {
		//画面描画
		Rect{ 0,0,300,1000 }.draw();
		Circle ball_det{ ball.Pos,20 };
		ball_det.draw(Palette::Orange);

		ClearPrint();
		Print <<U"score:" << score;
		for (int i(0); i < ranking.size(); i++) {//ランキングを順位をつけ表示
			Print << i+1 << U"：" << ranking[i];
		}
		

		trampoline_(ball_det);//トランポリン処理

		//ボール処理
		ball.move = { cos(Atan2(ball.move.y,ball.move.x)),sin(Atan2(ball.move.y,ball.move.x)) };
		ball.Pos += ball.move*Scene::DeltaTime()*ball_speed;

		ball_collision_object(ball_det);//ボールとobjectの衝突処理

		//objectの位置下げ
		for (int i(0); i < block.size();i++) {
			block[i].Pos = block[i].Pos + Vec2{0,Scene::DeltaTime() * block_speed};
		}

		//objectの追加
		if (block_addtimer.reachedZero()) {
			block_addtimer.restart();

			for (int i(0); i < 5; i++) {
				if (Random(1))continue;
				int rand(Random(2));

				//ランダムな形を追加
				Shape_Pos a;
				if (rand == 0)a.shape = U"rect";
				else if (rand == 1)a.shape = U"triangle";
				else if (rand == 2)a.shape = U"ellipse";

				a.Pos = { i * 100 + 300,-50 };

				block.push_back(a);
			}
		}

		//ボールと壁の衝突処理
		Line{ 300,0,300,800 }.draw(2,Palette::Skyblue);
		Line{ 300,0,800,0 }.draw(2,Palette::Skyblue);
		Line{ 800,0,800,800 }.draw(2,Palette::Skyblue);

		if (time > 0)time -= Scene::DeltaTime();
		else time = 0;

		//壁との接触処理
		auto points=ball_det.intersectsAt(Line{ 300,0,300,800 });
		if (not points)points = ball_det.intersectsAt(Line{ 300,0,800,0 });
		if (not points)points = ball_det.intersectsAt(Line{ 800,0,800,800 });
		if (points and time == 0)ball_reflection(ball_det, points), time = 0.02;

		//デッドラインとボールの衝突処理
		Line detline{ 300,795,800,795 };
		detline.draw(2,Palette::Red);

		if (ball_det.intersects(detline)) {//ボールとデッドライン
			game_over();
		}

		for (auto a : block) {//ブロックとデッドライン
			Shape shape = make_shape(a);
			if (shape.rect.intersects(detline) or shape.triangle.intersects(detline) or shape.ellipse.intersects(detline)) {
				game_over();
				break;
			}
		}
	}
}


/*===================-ゲームメモ-===========================================================

スペースキーまたは右クリックでゲームスタート、リスタート

マウスをドラッグしてトランポリンの配置

ブロックまたはボールが赤いラインに触れるとゲームオーバー

一応ランキングはあるけど実行すれば初期化されるからあんまり意味ない

==========================================================================================*/
