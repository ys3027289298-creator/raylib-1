/**********************************************************************************************
*
*   test_model_animation - Validate two-animation blending and time-based transitions
*
**********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TestModel {
    Model model;
    BoneInfo *bones;
    Transform *bindPose;
    ModelAnimPose currentPose;
    Matrix *boneMatrices;
} TestModel;

static int failureCount = 0;

static void Check(bool condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL: %s\n", message);
        failureCount++;
    }
}

static void CheckNear(float actual, float expected, float tolerance, const char *message)
{
    if (fabsf(actual - expected) > tolerance)
    {
        printf("FAIL: %s (actual: %.6f, expected: %.6f, tolerance: %.6f)\n",
            message, actual, expected, tolerance);
        failureCount++;
    }
}

static Transform IdentityTransform(void)
{
    Transform transform = { 0 };
    transform.rotation.w = 1.0f;
    transform.scale = (Vector3){ 1.0f, 1.0f, 1.0f };

    return transform;
}

static TestModel CreateTestModel(unsigned int boneCount)
{
    TestModel testModel = { 0 };

    testModel.bones = (BoneInfo *)calloc(boneCount, sizeof(BoneInfo));
    testModel.bindPose = (Transform *)calloc(boneCount, sizeof(Transform));
    testModel.currentPose = (Transform *)calloc(boneCount, sizeof(Transform));
    testModel.boneMatrices = (Matrix *)calloc(boneCount, sizeof(Matrix));

    for (unsigned int i = 0; i < boneCount; i++)
    {
        snprintf(testModel.bones[i].name, sizeof(testModel.bones[i].name), "bone%u", i);
        testModel.bones[i].parent = -1;
        testModel.bindPose[i] = IdentityTransform();
    }

    testModel.model.skeleton.boneCount = boneCount;
    testModel.model.skeleton.bones = testModel.bones;
    testModel.model.skeleton.bindPose = testModel.bindPose;
    testModel.model.currentPose = testModel.currentPose;
    testModel.model.boneMatrices = testModel.boneMatrices;

    return testModel;
}

static void UnloadTestModel(TestModel *testModel)
{
    free(testModel->bones);
    free(testModel->bindPose);
    free(testModel->currentPose);
    free(testModel->boneMatrices);
    memset(testModel, 0, sizeof(TestModel));
}

static ModelAnimation CreateTestAnimation(unsigned int boneCount, int keyframeCount)
{
    ModelAnimation anim = { 0 };

    anim.boneCount = boneCount;
    anim.keyframeCount = keyframeCount;
    anim.keyframePoses = (ModelAnimPose *)calloc(keyframeCount, sizeof(ModelAnimPose));

    for (int frame = 0; frame < keyframeCount; frame++)
    {
        anim.keyframePoses[frame] = (Transform *)calloc(boneCount, sizeof(Transform));
        for (unsigned int bone = 0; bone < boneCount; bone++) anim.keyframePoses[frame][bone] = IdentityTransform();
    }

    return anim;
}

static void UnloadTestAnimation(ModelAnimation *anim)
{
    for (int frame = 0; frame < anim->keyframeCount; frame++) free(anim->keyframePoses[frame]);
    free(anim->keyframePoses);
    memset(anim, 0, sizeof(ModelAnimation));
}

static float QuaternionDotProduct(Quaternion q1, Quaternion q2)
{
    return q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w;
}

static void SetBonePose(ModelAnimation *anim, int frame, unsigned int bone, Vector3 translation, Quaternion rotation, Vector3 scale)
{
    anim->keyframePoses[frame][bone].translation = translation;
    anim->keyframePoses[frame][bone].rotation = rotation;
    anim->keyframePoses[frame][bone].scale = scale;
}

static void TestWeightedBlending(void)
{
    TestModel testModel = CreateTestModel(3);
    ModelAnimation animA = CreateTestAnimation(3, 4);
    ModelAnimation animB = CreateTestAnimation(3, 8);

    Quaternion rotationA = QuaternionIdentity();
    Quaternion rotationB = QuaternionFromAxisAngle((Vector3){ 0.0f, 1.0f, 0.0f }, PI);
    Quaternion rotationSignTest = QuaternionFromAxisAngle((Vector3){ 0.0f, 1.0f, 0.0f }, 30.0f*DEG2RAD);
    Quaternion rotationSignTestFlipped = { -rotationSignTest.x, -rotationSignTest.y, -rotationSignTest.z, -rotationSignTest.w };

    for (int frame = 0; frame < animA.keyframeCount; frame++)
    {
        SetBonePose(&animA, frame, 0, (Vector3){ (float)(frame*2), 0.0f, 0.0f }, rotationA, (Vector3){ 1.0f, 1.0f, 1.0f });
        SetBonePose(&animA, frame, 1, (Vector3){ 0.0f, 0.0f, 0.0f }, rotationSignTest, (Vector3){ 1.0f, 1.0f, 1.0f });
        SetBonePose(&animA, frame, 2, (Vector3){ 4.0f, 0.0f, 0.0f }, rotationA, (Vector3){ 1.0f, 1.0f, 1.0f });
    }

    for (int frame = 0; frame < animB.keyframeCount; frame++)
    {
        SetBonePose(&animB, frame, 0, (Vector3){ 10.0f + (float)(frame*2), 0.0f, 0.0f }, rotationB, (Vector3){ 3.0f, 3.0f, 3.0f });
        SetBonePose(&animB, frame, 1, (Vector3){ 0.0f, 0.0f, 0.0f }, rotationSignTestFlipped, (Vector3){ 1.0f, 1.0f, 1.0f });
        SetBonePose(&animB, frame, 2, (Vector3){ -4.0f, 0.0f, 0.0f }, rotationA, (Vector3){ 1.0f, 1.0f, 1.0f });
    }

    const float weights[] = { 0.0f, 0.5f, 1.0f };
    const float expectedPositions[] = { 3.0f, 9.5f, 16.0f };
    const float expectedScales[] = { 1.0f, 2.0f, 3.0f };

    for (int i = 0; i < 3; i++)
    {
        UpdateModelAnimationEx(testModel.model, animA, 1.5f, animB, 3.0f, weights[i]);

        char message[128] = { 0 };
        snprintf(message, sizeof(message), "root translation at weight %.1f", weights[i]);
        CheckNear(testModel.currentPose[0].translation.x, expectedPositions[i], 0.0001f, message);

        snprintf(message, sizeof(message), "root scale at weight %.1f", weights[i]);
        CheckNear(testModel.currentPose[0].scale.x, expectedScales[i], 0.0001f, message);

        float rotationLength = QuaternionLength(testModel.currentPose[0].rotation);
        snprintf(message, sizeof(message), "root rotation normalized at weight %.1f", weights[i]);
        CheckNear(rotationLength, 1.0f, 0.0001f, message);

        float rotationAngle = 2.0f*acosf(fabsf(testModel.currentPose[0].rotation.w));
        snprintf(message, sizeof(message), "root rotation angle at weight %.1f", weights[i]);
        CheckNear(rotationAngle*RAD2DEG, 180.0f*weights[i], 0.1f, message);

        float rotationDot = QuaternionDotProduct(testModel.currentPose[1].rotation, rotationSignTest);
        snprintf(message, sizeof(message), "quaternion sign handled at weight %.1f", weights[i]);
        Check(rotationDot > 0.999f, message);
    }

    UpdateModelAnimationEx(testModel.model, animA, 5.5f, animB, 11.0f, 0.5f);
    CheckNear(testModel.currentPose[0].translation.x, 9.5f, 0.0001f, "independent wrapped clips blend at midpoint");

    UnloadTestAnimation(&animB);
    UnloadTestAnimation(&animA);
    UnloadTestModel(&testModel);
}

static void TestMissingBones(void)
{
    TestModel testModel = CreateTestModel(3);
    ModelAnimation animA = CreateTestAnimation(3, 4);
    ModelAnimation animB = CreateTestAnimation(1, 8);

    for (int frame = 0; frame < animA.keyframeCount; frame++)
    {
        SetBonePose(&animA, frame, 0, (Vector3){ 2.0f*(float)frame, 0.0f, 0.0f }, QuaternionIdentity(), (Vector3){ 1.0f, 1.0f, 1.0f });
        SetBonePose(&animA, frame, 1, (Vector3){ 0.0f, 4.0f, 0.0f }, QuaternionIdentity(), (Vector3){ 1.0f, 1.0f, 1.0f });
        SetBonePose(&animA, frame, 2, (Vector3){ 2.0f, 0.0f, 0.0f }, QuaternionIdentity(), (Vector3){ 1.0f, 1.0f, 1.0f });
    }

    for (int frame = 0; frame < animB.keyframeCount; frame++)
        SetBonePose(&animB, frame, 0, (Vector3){ 10.0f + 2.0f*(float)frame, 0.0f, 0.0f }, QuaternionIdentity(), (Vector3){ 1.0f, 1.0f, 1.0f });

    UpdateModelAnimationEx(testModel.model, animA, 1.5f, animB, 3.0f, 0.5f);
    CheckNear(testModel.currentPose[0].translation.x, 9.5f, 0.0001f, "matched bone blends normally");
    CheckNear(testModel.currentPose[1].translation.y, 2.0f, 0.0001f, "missing bone falls back to bind pose");
    CheckNear(testModel.currentPose[2].translation.x, 1.0f, 0.0001f, "partially matched bone uses bind pose fallback");
    Check(isfinite(testModel.currentPose[1].translation.y), "missing bone pose remains finite");

    UnloadTestAnimation(&animB);
    UnloadTestAnimation(&animA);
    UnloadTestModel(&testModel);
}

static void TestTransitionWeight(void)
{
    CheckNear(GetModelAnimationTransitionWeight(0.0f, 1.0f, MODEL_ANIMATION_EASING_LINEAR), 0.0f, 0.0001f, "linear transition starts at zero");
    CheckNear(GetModelAnimationTransitionWeight(0.5f, 1.0f, MODEL_ANIMATION_EASING_LINEAR), 0.5f, 0.0001f, "linear transition midpoint");
    CheckNear(GetModelAnimationTransitionWeight(1.0f, 1.0f, MODEL_ANIMATION_EASING_LINEAR), 1.0f, 0.0001f, "linear transition ends at one");
    CheckNear(GetModelAnimationTransitionWeight(-1.0f, 1.0f, MODEL_ANIMATION_EASING_SINE_IN_OUT), 0.0f, 0.0001f, "transition clamps negative time");
    CheckNear(GetModelAnimationTransitionWeight(2.0f, 1.0f, MODEL_ANIMATION_EASING_SINE_IN_OUT), 1.0f, 0.0001f, "transition clamps elapsed time");
    CheckNear(GetModelAnimationTransitionWeight(0.5f, 1.0f, MODEL_ANIMATION_EASING_SINE_IN_OUT), 0.5f, 0.0001f, "sine in-out midpoint");
    CheckNear(GetModelAnimationTransitionWeight(0.5f, 1.0f, MODEL_ANIMATION_EASING_SMOOTHSTEP), 0.5f, 0.0001f, "smoothstep midpoint");
    CheckNear(GetModelAnimationTransitionWeight(0.0f, 0.0f, MODEL_ANIMATION_EASING_LINEAR), 1.0f, 0.0001f, "zero-duration transition is immediate");

    ModelAnimationEasing easings[] = {
        MODEL_ANIMATION_EASING_LINEAR,
        MODEL_ANIMATION_EASING_SINE_IN,
        MODEL_ANIMATION_EASING_SINE_OUT,
        MODEL_ANIMATION_EASING_SINE_IN_OUT,
        MODEL_ANIMATION_EASING_CUBIC_IN,
        MODEL_ANIMATION_EASING_CUBIC_OUT,
        MODEL_ANIMATION_EASING_CUBIC_IN_OUT,
        MODEL_ANIMATION_EASING_SMOOTHSTEP
    };

    for (int i = 0; i < 8; i++)
    {
        float previous = -1.0f;
        for (int sample = 0; sample <= 20; sample++)
        {
            float blend = GetModelAnimationTransitionWeight((float)sample/20.0f, 1.0f, easings[i]);
            Check((blend >= 0.0f) && (blend <= 1.0f), "easing weight stays in range");
            Check(blend >= previous, "easing weight is monotonic");
            previous = blend;
        }
        CheckNear(GetModelAnimationTransitionWeight(0.0f, 1.0f, easings[i]), 0.0f, 0.0001f, "easing starts at zero");
        CheckNear(GetModelAnimationTransitionWeight(1.0f, 1.0f, easings[i]), 1.0f, 0.0001f, "easing ends at one");
    }
}

static void TestTransitionRecording(void)
{
    TestModel testModel = CreateTestModel(2);
    ModelAnimation animA = CreateTestAnimation(2, 4);
    ModelAnimation animB = CreateTestAnimation(2, 8);

    for (int frame = 0; frame < animA.keyframeCount; frame++)
    {
        SetBonePose(&animA, frame, 0, (Vector3){ 2.0f*(float)frame, 0.0f, 0.0f }, QuaternionIdentity(), (Vector3){ 1.0f, 1.0f, 1.0f });
        SetBonePose(&animA, frame, 1, (Vector3){ 0.0f, 1.0f, 0.0f }, QuaternionIdentity(), (Vector3){ 1.0f, 1.0f, 1.0f });
    }

    for (int frame = 0; frame < animB.keyframeCount; frame++)
    {
        SetBonePose(&animB, frame, 0, (Vector3){ 10.0f + 2.0f*(float)frame, 0.0f, 0.0f },
            QuaternionFromAxisAngle((Vector3){ 0.0f, 1.0f, 0.0f }, PI), (Vector3){ 2.0f, 2.0f, 2.0f });
        SetBonePose(&animB, frame, 1, (Vector3){ 0.0f, 3.0f, 0.0f }, QuaternionIdentity(), (Vector3){ 1.0f, 1.0f, 1.0f });
    }

    const float duration = 0.5f;
    const int sampleCount = 61;
    const float frameA = 1.5f;
    const float frameB = 3.0f;
    float recordedPositions[61] = { 0 };
    float maxDelta = 0.0f;

    UpdateModelAnimation(testModel.model, animA, frameA);
    float initialPosition = testModel.currentPose[0].translation.x;

    for (int i = 0; i < sampleCount; i++)
    {
        float elapsedTime = duration*(float)i/(float)(sampleCount - 1);
        UpdateModelAnimationTransition(testModel.model, animA, frameA, animB, frameB,
            duration, elapsedTime, MODEL_ANIMATION_EASING_SINE_IN_OUT);

        recordedPositions[i] = testModel.currentPose[0].translation.x;
        if (i > 0)
        {
            float delta = fabsf(recordedPositions[i] - recordedPositions[i - 1]);
            if (delta > maxDelta) maxDelta = delta;
        }

        CheckNear(QuaternionLength(testModel.currentPose[0].rotation), 1.0f, 0.0001f, "transition rotation normalized");
    }

    CheckNear(recordedPositions[0], initialPosition, 0.0001f, "transition starts from source pose");
    CheckNear(recordedPositions[sampleCount - 1], 16.0f, 0.0001f, "transition ends at destination pose");
    Check(maxDelta < 0.4f, "transition has no sampled position jump");

    float previousPosition = recordedPositions[0];
    for (int i = 1; i < sampleCount; i++)
    {
        Check(recordedPositions[i] >= previousPosition, "transition position is monotonic");
        previousPosition = recordedPositions[i];
    }

    UpdateModelAnimation(testModel.model, animB, frameB);
    CheckNear(testModel.currentPose[0].translation.x, recordedPositions[sampleCount - 1], 0.0001f,
        "switching to single animation has no endpoint jump");

    printf("Transition recording: %d samples, max position delta: %.6f\n", sampleCount, maxDelta);

    UnloadTestAnimation(&animB);
    UnloadTestAnimation(&animA);
    UnloadTestModel(&testModel);
}

int main(void)
{
    TestWeightedBlending();
    TestMissingBones();
    TestTransitionWeight();
    TestTransitionRecording();

    if (failureCount == 0)
    {
        printf("All model animation blending tests passed\n");
        return EXIT_SUCCESS;
    }

    printf("%d model animation blending test(s) failed\n", failureCount);
    return EXIT_FAILURE;
}
