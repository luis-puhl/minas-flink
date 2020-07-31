# MFOG_k=10 \
# MFOG_dimension=2 \
# MFOG_TRAINING_CSV=datasets/syn-training.csv \
# MFOG_MODEL_CSV=datasets/model-clean.csv \
# MFOG_EXAMPLES_CSV=datasets/test.csv \
# MFOG_MATCHES_CSV=out/matches.csv \
# MFOG_TIMING_LOG=stdout \
make target/mfog
./target/mfog --cloud k=10 dimension=2 MODEL_CSV datasets/syn-0-initial.csv TIMING_LOG /dev/null
sleep 1
./target/mfog k=10 dimension=2 EXAMPLES_CSV datasets/test.csv MATCHES_CSV=out/syn-matches.csv TIMING_LOG /dev/null
