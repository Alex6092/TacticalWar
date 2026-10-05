#include <doctest.h>

#include <ChallengeBoard.h>

using tw::ChallengeBoard;

TEST_CASE("A challenge waits 30 seconds for an answer, then expires")
{
	ChallengeBoard board;
	CHECK(board.challenge(1, 2, false, true, true, 1000).empty());
	REQUIRE(board.receivedBy(2) != nullptr);
	CHECK(board.receivedBy(2)->from == 1);
	CHECK(board.sentBy(1) != nullptr);

	CHECK(board.expire(1000 + ChallengeBoard::TIMEOUT_MS - 1).empty());
	std::vector<ChallengeBoard::Challenge> expired = board.expire(1000 + ChallengeBoard::TIMEOUT_MS);
	REQUIRE(expired.size() == 1);
	CHECK(expired[0].to == 2);
	CHECK(board.receivedBy(2) == nullptr);
	// Trop tard pour répondre.
	CHECK_FALSE(board.answer(2, 1, 1000 + ChallengeBoard::TIMEOUT_MS + 5));
}

TEST_CASE("A team has one challenge at a time and cannot challenge itself or a busy team")
{
	ChallengeBoard board;
	CHECK_FALSE(board.challenge(1, 1, false, true, true, 0).empty());
	CHECK_FALSE(board.challenge(1, 2, false, false, true, 0).empty());
	CHECK_FALSE(board.challenge(1, 2, false, true, false, 0).empty());
	CHECK(board.challenge(1, 2, false, true, true, 0).empty());
	// Défi en double, défi d'une équipe déjà défiée, défi par l'équipe qui attend une réponse.
	CHECK_FALSE(board.challenge(1, 2, false, true, true, 10).empty());
	CHECK_FALSE(board.challenge(3, 2, false, true, true, 10).empty());
	CHECK_FALSE(board.challenge(1, 3, false, true, true, 10).empty());
	CHECK_FALSE(board.challenge(2, 3, false, true, true, 10).empty());
	CHECK_FALSE(board.challenge(3, 1, false, true, true, 10).empty());
	// D'autres équipes restent libres.
	CHECK(board.challenge(3, 4, false, true, true, 10).empty());
	CHECK(board.pending().size() == 2);
}

TEST_CASE("No challenge while a tournament is running")
{
	ChallengeBoard board;
	std::string refusal = board.challenge(1, 2, true, true, true, 0);
	CHECK_FALSE(refusal.empty());
	CHECK(refusal.find("tournoi") != std::string::npos);
	CHECK(board.pending().empty());

	// Un tournoi qui commence retire les défis en attente.
	CHECK(board.challenge(1, 2, false, true, true, 0).empty());
	CHECK(board.cancelAll().size() == 1);
	CHECK(board.pending().empty());
}

TEST_CASE("The first teammate to answer decides for the team")
{
	ChallengeBoard board;
	CHECK(board.challenge(1, 2, false, true, true, 0).empty());
	ChallengeBoard::Challenge answered;
	CHECK(board.answer(2, 1, 100, &answered));
	CHECK(answered.from == 1);
	CHECK(answered.to == 2);
	// Le coéquipier répond trop tard : le défi n'existe plus.
	CHECK_FALSE(board.answer(2, 1, 120));
	// Une réponse au mauvais défi ne compte pas.
	CHECK(board.challenge(3, 4, false, true, true, 200).empty());
	CHECK_FALSE(board.answer(4, 1, 210));
	CHECK(board.receivedBy(4) != nullptr);
	CHECK(board.cancelInvolving(3).size() == 1);
	CHECK(board.receivedBy(4) == nullptr);
}
