float camRotation = 15.0f;
double currentTime = 0.0;
double respawnTime = 0.0;
uint32_t score = 0;
uint32_t bestScore = 0;

short countDown = 0;

const float JUMP_SPEED = 0.05f;
bool isGoing = true;

void managePhysics();

void createPhysicsThread()
{
    std::thread physicsThread(managePhysics);
    physicsThread.detach();
}

void resetEverything()
{
  currentTime = 0.0f;
  score = 0;
  uint32_t badId = 0;
  for (auto& gameObject : gameObjects)
  {
    if (gameObject.name.compare("FlappyBird") == 0)
    {
      gameObject.position = glm::vec3(0.0f, 0.0f, 10.0f);
      gameObject.velocity = glm::vec3(0.0f);
      gameObject.rotation.x = 0.0f;
    }
    if (gameObject.isBad == true)
    {
      gameObject.position = glm::vec3(
          30.0f + static_cast<float>(badId) * (200.0f / numberOfTubes),
          0.0f,
          6.0f + 10.5f * 0.01f * static_cast<float>(rand() % 100));

      gameObject.scale.z = 1.2f;

      gameObject.isScored = false;

      badId++;
    }
  }
}

void jump()
{
  for (auto& gameObject : gameObjects)
  {
    if (gameObject.name.compare("FlappyBird") == 0)
    {
      gameObject.velocity.z = JUMP_SPEED;
    }
  }
}

void die()
{
  countDown = 0;
  isGoing = false;
  respawnTime = currentTime + 5.0;
  if (bestScore < score)
    bestScore = score;
  std::cout << "You Died! Score: " << score << ". Best Score: " << bestScore << "\n";
}

void managePhysics()
{
  using Framerate = std::chrono::duration<std::chrono::steady_clock::rep, std::ratio<1, PHYSICS_FPS>>;
  auto next = std::chrono::steady_clock::now() + Framerate{1};
  while (true)
  {
    if (currentTime > respawnTime && isGoing == false)
    {
      resetEverything();
      isGoing = true;
    }
    if (currentTime > 3.0 && isGoing)
    {
      if (countDown == 3)
      {
        std::cout << "Start!\n";
        jump();
        countDown++;
      }
      for (auto& gameObject : gameObjects)
      {
        gameObject.rotation += gameObject.rotationSpeed;
        gameObject.velocity += gameObject.accel;
        gameObject.position += gameObject.velocity;

        if (gameObject.name.compare("FlappyBird") == 0)
        {
          if (gameObject.position.z <= 1.1f)
          {
            die();
            // bouncing instead of dying
            //gameObject.position.z = 0.74f;
            //if (gameObject.velocity.z < 0.0f)
            //{
            //  gameObject.velocity.z = -gameObject.velocity.z;
            //}
          }
          gameObject.rotation.x = -std::atan(gameObject.velocity.z * 10.0f) * 90.0f;

          for (auto& gameObject2 : gameObjects)
          {
            if (gameObject2.isBad == 1)
            {
              if (gameObject2.position.x <= -2.15f)
              {
                if (!gameObject2.isScored)
                {
                  score++;
                  gameObject2.isScored = true;
                  std::cout << "score: " << score << "\n";
                }
              }

              if (gameObject2.position.x < 2.9f &&
                  gameObject2.position.x > -2.32f)
              {
                if (gameObject.position.z - gameObject2.position.z > 3.57f * gameObject2.scale.z - 0.97f ||
                    gameObject.position.z - gameObject2.position.z < -3.57f * gameObject2.scale.z + 0.97f)
                {
                  die();
                }
              }
            }
          }
        }
        else if (gameObject.isBad == true)
        {
          if (gameObject.position.x <= -40.0f)
          {
            gameObject.position.x = 160.0f;
            gameObject.scale.z = 1.2f - 0.26f * std::atan((currentTime - 12.5) * 0.02);
            gameObject.position.z = 6.0f + 10.5f * 0.01f * static_cast<float>(rand() % 100);
            gameObject.isScored = false;
          }
          gameObject.velocity.x = -(0.015f + std::atan(currentTime * 0.01) * 0.005f);
        }
      }
    }
    else if (currentTime < 3.0)
    {
      if (static_cast<double>(countDown) < currentTime)
      {
        countDown++;
        std::cout << countDown << "\n";
      }
    }

    currentTime += (1.0 / static_cast<double>(PHYSICS_FPS));

    std::this_thread::sleep_until(next);
    next += Framerate{1};
  }
}
