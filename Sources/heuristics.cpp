/* Array-Generator by Isaac Jung
Last updated 10/12/2022

|===========================================================================================================|
|   This file contains definitions for methods belonging to the Array class which are declared in array.h.  |
| Specifically, any methods involving heuristics for scoring and altering a potential row to add are found  |
| here. The only reason they are not in array.cpp is for organization.                                      |
|===========================================================================================================|
*/

#include "array.h"

/* SUB METHOD: add_row - adds a new row to the array using some predictive and scoring logic
 * - simply an interface for adding a row; method itself simply decides which heuristic to use
 * 
 * returns:
 * - void, but after the method finishes, the array will have a new row appended to its end
*/
void Array::add_row()
{
    // choose a new random order for the column iterations this round
    for (uint64_t size = num_factors; size > 0; size--) {
        int rand_idx = rand() % static_cast<int>(size);
        int temp = permutation[size - 1];
        permutation[size - 1] = permutation[rand_idx];
        permutation[rand_idx] = temp;
    }   // at this point, permutation should be shuffled

    // choose how to initialize the new row based on current heuristic to be used
    int *new_row;
    T *locked = nullptr;
    switch (heuristic_in_use) {
        case c_only:
        case c_and_l:
        case c_and_d:
            new_row = initialize_row_S();
            break;
        case l_only:
        case l_and_d:
            new_row = initialize_row_T(&locked);
            break;
        case d_only:
            // TODO: implement initialize_row_I() and call that here, then break
        case all:
        case none:
        default:
            new_row = initialize_row_R();
            break;
    }   // at this point, new row should be initialized with values
    
    // tweak the row based on the current heuristic and then add to the array
    tweak_row(new_row, locked);
    update_array(new_row);
}

/* SUB METHOD: initialize_row_R - creates a randomly generated row
 * 
 * returns:
 * - a pointer to the first element in the array that represents the row
*/
int *Array::initialize_row_R()
{
    int *new_row = new int[num_factors];
    for (uint64_t i = 0; i < num_factors; i++)
        new_row[i] = static_cast<uint64_t>(rand()) % factors[i]->level;
    return new_row;
}

/* SUB METHOD: initialize_row_S - creates a row by considering which Singles have the most issues
 * 
 * returns:
 * - a pointer to the first element in the array that represents the row
*/
int* Array::initialize_row_S()
{
    int *new_row = new int[num_factors]{0};

    // greedily select the values that appear to need the most attention
    for (uint64_t col = 0; col < num_factors; col++) {
        // check if column is don't care
        if ((p == all && dont_cares[permutation[col]] == all) ||
            (p == c_and_l && dont_cares[permutation[col]] == c_and_l) ||
            (p == c_only && dont_cares[permutation[col]] == c_only)) {
            new_row[permutation[col]] = static_cast<uint64_t>(rand()) % factors[permutation[col]]->level;
            continue;
        }
        // assume 0 is the worst to start, then check if any others are worse
        Single *worst_single = factors[permutation[col]]->singles[0];
        int worst_score = static_cast<int64_t>(worst_single->c_issues) + worst_single->l_issues + 
            3*static_cast<int64_t>(worst_single->d_issues);
        for (uint64_t val = 1; val < factors[permutation[col]]->level; val++) {
            Single *cur_single = factors[permutation[col]]->singles[val];
            int cur_score = static_cast<int64_t>(cur_single->c_issues) + cur_single->l_issues +
                3*static_cast<int64_t>(cur_single->d_issues);
            if (cur_score > worst_score || (cur_score == worst_score && rand() % 2 == 0)) {
                worst_single = cur_single;
                worst_score = cur_score;
            }
        }
        new_row[permutation[col]] = worst_single->value;
    }   // entire row is now initialized based on the greedy approach
    return new_row;
}

/* SUB METHOD: initialize_row_T - creates a row by considering which T sets have the most location conflicts
 * 
 * returns:
 * - a pointer to the first element in the array that represents the row
*/
int *Array::initialize_row_T(T **locked)
{
    int *new_row = initialize_row_R();
    
    int64_t worst_count = INT64_MIN;
    std::vector<T*> worst_sets; // there could be ties for the worst
    for (T *t_set : sets) {
        //t_set->rows.size()==0
        if (static_cast<int64_t>(t_set->location_conflicts.size()) >= worst_count) {    // worse or tied
            if (static_cast<int64_t>(t_set->location_conflicts.size()) > worst_count) { // strictly worse
                worst_count = t_set->location_conflicts.size();
                worst_sets.clear();
            }
            worst_sets.push_back(t_set);
        }
    }

    // choose the set with most conflicts (for ties, choose randomly from among those tied for the worst)
    *locked = worst_sets.at(static_cast<uint64_t>(rand()) % worst_sets.size());
    for (Single *s : (*locked)->singles) new_row[s->factor] = s->value;
    return new_row;
}

/* SUB METHOD: initialize_row_I - creates a row by considering which Interactions have the lowest separation
 * 
 * returns:
 * - a pointer to the first element in the array that represents the row
*/
int *Array::initialize_row_I()
{
    int *new_row = new int[num_factors];
    // TODO: logic
    return new_row;
}

/* SUB METHOD: tweak_row - chooses a heuristic to use for modifying a row based on current state of the Array
 * 
 * parameters:
 * - row: integer array representing a row being considered for adding to the array
 * 
 * returns:
 * - void, but after the method finishes, the row may be modified in an attempt to satisfy more issues
*/
void Array::tweak_row(int *row, T *locked)
{
    switch (heuristic_in_use) {
        case c_only:
        case c_and_l:
        case c_and_d:
            heuristic_c_only(row);
            break;
        case l_only:
        case l_and_d:
            heuristic_l_only(row, locked);
            break;
        case d_only:
            heuristic_d_only(row);
            break;
        case all:
            heuristic_all(row);
            break;
        case none:
        default:
            break;
    }
}

/* SUB METHOD: heuristic_c_only - lightweight heuristic that only concerns itself with coverage
 * - in the tradeoff between speed and better row choice, this heuristic is towards the speed extreme
 * - should only be used very early on in array construction
 * --> can do well for longer when the desired array is simpler (i.e., covering as opposed to detecting)
 * 
 * parameters:
 * - row: integer array representing a row being considered for adding to the array
 * 
 * returns:
 * - void, but after the method finishes, the row may be modified in an attempt to satisfy more issues
*/
void Array::heuristic_c_only(int *row)
{
    int *problems = new int[num_factors]{0};    // for counting how many "problems" each factor has
    int max_problems;   // largest value among all in the problems[] array created above
    int cur_max;    // for comparing to max_problems to see if there is an improvement
    prop_mode *dont_cares_c = new prop_mode[num_factors];   // local copy of the don't cares
    for (uint64_t col = 0; col < num_factors; col++) dont_cares_c[col] = dont_cares[col];

    std::set<Interaction*> row_interactions;
    build_row_interactions(row, &row_interactions, 0, t, "");
    for (Interaction *i : row_interactions) {
        if (i->rows.size() != 0) {  // Interaction is already covered
            bool can_skip = false;  // don't account for Interactions involving already-completed factors
            for (Single *s : i->singles)
                if (dont_cares_c[s->factor] != none) {
                    can_skip = true;
                    break;
                }
            if (can_skip) continue;
            for (Single *s : i->singles) // increment the problems counter for each Single involved
                problems[s->factor]++;
        } else {    // Interaction not covered; decrement the problems counters instead
            for (Single *s : i->singles) problems[s->factor]--;
        }
    }

    // find out what the worst score is among the factors
    max_problems = 0;
    for (uint64_t col = 0; col < num_factors; col++)
        if (problems[col] > max_problems) max_problems = problems[col];
    if (max_problems == 0) {    // row is good enough as is
        delete[] problems;
        delete[] dont_cares_c;
        return;
    }
    
    // else, try altering the value(s) with the most problems (whatever is currently contributing the least)
    cur_max = max_problems;
    for (uint64_t col = 0; col < num_factors; col++) {  // go find any factors to change
        if (problems[permutation[col]] == max_problems) {   // found a factor to try altering
            int *temp_problems = new int[num_factors]{0};   // deep copy problems[] because it will be mutated

            for (uint64_t i = 1; i < factors[permutation[col]]->level; i++) {   // for every value
                row[permutation[col]] = (row[permutation[col]] + 1) %
                    static_cast<int64_t>(factors[permutation[col]]->level); // try that value
                std::set<Interaction*> new_interactions;    // get the new Interactions
                build_row_interactions(row, &new_interactions, 0, t, "");

                cur_max = heuristic_c_helper(row, &new_interactions, temp_problems);    // test this change
                if (cur_max < max_problems) {   // this change improved the score, keep it
                    delete[] problems;
                    delete[] dont_cares_c;
                    delete[] temp_problems;
                    return;
                }
                cur_max = max_problems; // else this change was no good, reset and continue
            }
            delete[] temp_problems;
            row[permutation[col]] = (row[permutation[col]] + 1) % static_cast<int>(factors[col]->level);
        }
    }

    // last resort, start looking for *anything* that is missing
    for (uint64_t col = 0; col < num_factors; col++) {  // for all factors
        if (dont_cares_c[permutation[col]] != none) continue;   // no need to check already completed factors
        bool improved = false;
        for (uint64_t i = 0; i < factors[permutation[col]]->level; i++) {   // for every value
            row[permutation[col]] = (row[permutation[col]] + 1) %
                static_cast<int64_t>(factors[permutation[col]]->level); // try that value
            std::set<Interaction*> new_interactions;    // get the new Interactions
            build_row_interactions(row, &new_interactions, 0, t, "");

            improved = false;   // see if the change helped
            for (Interaction *interaction : new_interactions)
                if (interaction->rows.size() == 0) {    // the Interaction is not already covered
                    for (Single *s : interaction->singles) dont_cares_c[s->factor] = c_only;
                    improved = true;
                }
            if (improved) break;    // keep this factor as this value
        }
        if (improved) continue;
        row[permutation[col]] = static_cast<uint64_t>(rand()) % factors[permutation[col]]->level;
    }
    delete[] problems;
    delete[] dont_cares_c;
}

/* HELPER METHOD: heuristic_c_helper - performs redundant work for heuristic_c_only()
 * 
 * parameters:
 * - row: integer array representing a row being considered for adding to the array
 * - row_interactions: set containing all Interactions present in the row
 * - problems: pointer to start of array associating each column in the row with a score of sorts
 * 
 * returns:
 * - int representing the largest value in the problems array after scoring
*/
int Array::heuristic_c_helper(int *row, std::set<Interaction*> *row_interactions, int *problems)
{
    for (Interaction *i : *row_interactions) {
        if (i->rows.size() != 0) {  // Interaction is already covered
            bool can_skip = false;  // don't account for Interactions involving already-completed factors
            for (Single *s : i->singles)
                if (s->c_issues == 0) { // one of the Singles involved in the Interaction is completed
                    // TODO: make this check more than just c_issues?
                    can_skip = true;
                    break;
                }
            if (can_skip) continue;
            for (Single *s : i->singles) // increment the problems counter for each Single involved
                problems[s->factor]++;
        } else {    // Interaction not covered; decrement the problems counters instead
            for (Single *s : i->singles) problems[s->factor]--;
        }
    }

    // find out what the worst score is among the factors
    int max_problems = INT32_MIN;   // set max to a huge negative number to start
    for (uint64_t col = 0; col < num_factors; col++) {
        if (factors[col]->singles[row[col]]->c_issues == 0) continue;   // already completed factor
        if (problems[col] > max_problems) max_problems = problems[col];
    }
    return max_problems;
}

/* SUB METHOD: heuristic_l_only - middleweight heuristic that only concerns itself with location
 * - in the tradeoff between speed and better row choice, this heuristic is somewhere in the middle
 * - should be used when most, if not all, coverage problems have been solved
 * 
 * parameters:
 * - row: integer array representing a row being considered for adding to the array
 * 
 * returns:
 * - void, but after the method finishes, the row may be modified in an attempt to satisfy more issues
*/
void Array::heuristic_l_only(int *row, T *locked)
{
    // keep track of which columns should not be modified
    bool *locked_factors = new bool[num_factors]{false};
    for (Single *s : locked->singles) locked_factors[s->factor] = true;

    std::map<std::string, uint64_t> scores; // create and initialize a map of every Single to a scoring
    for (uint64_t col = 0; col < num_factors; col++)
        for (uint64_t val = 0; val < factors[col]->level; val++)
            scores.insert({"f" + std::to_string(col) + "," + std::to_string(val), 0});
    
    for (T *conflict : locked->location_conflicts)  // for every conflicting T set,
        for (Single *s : conflict->singles) // for every Single in that conflicting set,
            scores.at(s->to_string())++;    // increase the score of that Single

    // a larger value in the scores map means the Single is involved in more location conflicts
    for (uint64_t col = 0; col < num_factors; col++) {
        if (locked_factors[col]) continue;
        uint64_t best_val;
        uint64_t best_val_score = 0;
        for (uint64_t val = 0; val < factors[col]->level; val++) {
            uint64_t val_score = scores.at("f" + std::to_string(col) + "," + std::to_string(val));
            if (val_score > best_val_score) {
                best_val = val;
                best_val_score = val_score;
            }
        }
        if (best_val_score != 0) row[col] = best_val;   // else allow it to remain random
    }
    delete[] locked_factors;
}

/* SUB METHOD: heuristic_d_only - middleweight heuristic that only concerns itself with detection
 * - in the tradeoff between speed and better row choice, this heuristic is somewhere in the middle
 * - should be used when detection problems are all that remain
 * 
 * parameters:
 * - row: integer array representing a row being considered for adding to the array
 * 
 * returns:
 * - void, but after the method finishes, the row may be modified in an attempt to satisfy more issues
*/
void Array::heuristic_d_only(int *row)
{

}

/* SUB METHOD: heuristic_all - heavyweight heuristic that tries to solve the most problems possible
 * - in the tradeoff between speed and better row choice, this heuristic is towards the row choice extreme
 * - does the deepest inspection of all the heuristics; therefore, should not be used till close to complete
 * 
 * parameters:
 * - row: integer array representing a row up for consideration for appending to the array
 * 
 * returns:
 * - none, but the row will be altered such that it solves as many problems singlehandedly as possible
 *  --> note that this does not mean that running this for the whole array will guarantee the smallest array;
 *      this is still a greedy algorithm for the current row, without any lookahead to future rows
*/
void Array::heuristic_all(int *row)
{
    // get scores for all relevant possible rows
    std::map<int*, int64_t> scores;
    heuristic_all_helper(row, 0, &scores);
    //TODO: wait for all child processes to terminate (once threading has been implemented)

    // inspect the scores for the best one(s)
    int64_t best_score = INT64_MIN;
    std::vector<int*> best_rows;    // there could be ties for the best
    for (auto& kv : scores) {
        if (kv.second >= best_score) {  // it was better or it tied
            if (kv.second > best_score) {   // for an even better choice, can stop tracking the previous best
                for (int *r : best_rows) delete[] r;
                best_score = kv.second;
                best_rows.clear();
            }
            best_rows.push_back(kv.first);  // whether it was better or only a tie, keep track of this row
        } else delete[] kv.first;
    }

    // choose the row that scored the best (for ties, choose randomly from among those tied for the best)
    int choice = static_cast<uint64_t>(rand()) % best_rows.size(); // for breaking ties randomly
    for (uint64_t col = 0; col < num_factors; col++)
        row[col] = best_rows.at(choice)[col];
    
    // free memory
    for (int *r : best_rows) delete[] r;
}

/* HELPER METHOD: heuristic_all_helper - performs top-down recursive logic for heuristic_all()
 * - heuristic_all() does the auxilary work to start the recursion, and handle the result
 * - this method uses recursion to form all possible combinations; its base case scores a given combination
 * 
 * parameters:
 * - row: integer array representing a row being considered for adding to the array
 * - cur_col: which column should have its levels looped over in the recursive case
 * --> overhead caller should pass 0 to this method initially
 * --> value should increment by 1 with each recursive call
 * --> triggers the base case when value is equal to the total number of columns
 * - scores: pointer to a map whose keys are pointers to rows and whose values are the scores of those rows
 * --> overhead caller should pass the address of an empty map to this method initially
 * --> for each row inspected by the base case, a separate thread should handle the scoring and map updating
 * 
 * returns:
 * - none, but scores will be modified to contain all the rows inspected and their scores
*/
void Array::heuristic_all_helper(int *row, uint64_t cur_col, std::map<int*, int64_t> *scores)
{
    // base case: row represents a unique combination and is ready for scoring
    if (cur_col == num_factors) {

        int *row_copy = new int[num_factors];   // must be deleted by heuristic_all() later
        for (uint64_t col = 0; col < num_factors; col++) row_copy[col] = row[col];
        heuristic_all_scorer(row_copy, scores);
        return;
    }

    // recursive case: need to introduce another loop for the next factor
    /*
    if ((p == all && dont_cares[permutation[cur_col]] == all) ||
        (p == c_and_l && dont_cares[permutation[cur_col]] == c_and_l) ||
        (p == c_only && dont_cares[permutation[cur_col]] == c_only)) {
        heuristic_all_helper(row, cur_col+1, scores);
        return;
    }//*/
    for (uint64_t offset = 0; offset < factors[permutation[cur_col]]->level; offset++) {
        int temp = row[permutation[cur_col]];
        row[permutation[cur_col]] = (row[permutation[cur_col]] + static_cast<int>(offset)) %
            static_cast<int>(factors[permutation[cur_col]]->level); // try every value for this factor
        heuristic_all_helper(row, cur_col+1, scores);
        row[permutation[cur_col]] = temp;
    }
}

/* HELPER METHOD: heuristic_all_scorer - scores a given row by testing what would change if it was added
 * - should be called in a unique thread
 * - heuristic_all() should await the termination of all sub threads before inspecting scores
 * 
 * parameters:
 * - row_v: a vector representing the row to be scored
 *  --> cannot be a pointer to the row being modified by heuristic_all_helper() (would result in data races)
 * - scores: pointer to 
 * 
 * returns:
 * - none, but best_rows will be modified to contain whichever row(s) scored best
 * --> also, best_score will be modified, but this value will likely not be needed by the caller
*/
void Array::heuristic_all_scorer(int *row, std::map<int*, int64_t> *scores)
{
    // current thread will work with unique copies of the data structures being modified
    Array *copy = clone();
    copy->update_array(row, false); // see how all scores, etc., would change

    // define the row score to be the combination of net changes below, weighted by importance
    int64_t row_score = 0; //= prev_score - static_cast<int64_t>(score);
    for (Single *this_s : singles) { // improve the score based on individual Single improvement
        Single *copy_s = copy->single_map.at(this_s->to_string());
        uint64_t weight = (factors[this_s->factor]->level); // higher level factors hold more weight
        row_score += static_cast<int64_t>(weight*(this_s->c_issues - copy_s->c_issues));
        row_score += 2*weight*(this_s->l_issues - copy_s->l_issues);
        row_score += static_cast<int64_t>(3*weight*(this_s->d_issues - copy_s->d_issues));
    }

    // need to add result to data structure containing all thread's results; use mutex for thread safety
    scores_mutex.lock();
    scores->insert({row, row_score});
    scores_mutex.unlock();

    delete copy;
    // note: do not delete row here, as heuristic_all() will need to reference it
}