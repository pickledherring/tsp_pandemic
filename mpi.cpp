#include <vector>
#include <map>
#include <limits>
#include <string>
#include <iostream>
#include <math.h>
#include <mpi.h>

struct Vertex {
    float weight;
    int name;
    float coords[2];
    Vertex* parent;
    std::vector<Vertex*> children;
};

struct Edge {
    float weight;
    Vertex* start, * end;
};

int MST(std::vector<Vertex*> vertexes, int names[], int len_names) {
    std::vector<Vertex*> verts;
    int rank, n_processes;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_processes);
    for (int i = 0; i < len_names; i++) {
        for (int j = 0; j < vertexes.size(); j++) { // would be better if we sorted names
            if (vertexes[j]->name == names[i]) {
                verts.push_back(vertexes[j]);
            }
        }
    }

    std::vector<Edge> edges;
    std::vector<bool> selected(verts.size(), false);
    selected[0] = true;

    //building our MST
    float min_in_sector = 1.1; // minimum weight for this thread
    int min_index;
    for (int i = 0; i < verts.size(); i++) {
        if (verts[i]->weight < min_in_sector) {
            min_in_sector = verts[i]->weight;
            min_index = i;
        }
    }

    while (edges.size() < verts.size() - 1) {
        float minimum = 1.1; // each probability product's max is 1
        Edge to_add;

        for (int i = 0; i < selected.size(); i++) {
            if (selected[i]) {
                for (int j = 0; j < selected.size(); j++) {
                    if ((i != j) && !selected[j]) {
                        float weight = verts[i]->weight * verts[j]->weight;
                        if (weight < minimum) {
                            minimum = weight;
                            to_add.weight = weight;
                            to_add.start = verts[i];
                            to_add.end = verts[j];
                        }
                    }
                }
            }
        }

        for (int i = 0; i < verts.size(); i++) {
            if (verts[i]->name == to_add.start->name) {
                verts[i]->children.push_back(to_add.end);
            }
            if (verts[i]->name == to_add.end->name) {
                verts[i]->parent = to_add.start;
                selected[i] = true;
            }
        }
        edges.push_back(to_add);
    }

    return min_index;
}

void gather_verts(std::vector<Vertex*> vert_cuts[], int n_processes) {
    int max = 0;
    for (int i = 1; i < n_processes; i ++) {
        if (vert_cuts[i].size() > max) {
            max = vert_cuts[i].size();
        }
    }
    std::cout<<"max: "<<max<<std::endl;
    int start_pos_send;
    int start_pos_rec;
    int end_pos_send[max];
    int end_pos_rec[max];
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cout<<"rank "<<rank<<" made it!"<<std::endl;
    MPI_Status Stat;

    if (rank == 0) {
        for (int i = 1; i < n_processes; i++) {
            std::cout<<"vert_cuts["<<i<<"].size(): "<<vert_cuts[i].size();
            for (int j = 0; j < vert_cuts[i].size(); j++) { // wait for other vertexes to process
                std::cout<<"vert_cuts["<<i<<"]["<<j<<"]: "<<vert_cuts[i][j]->name<<std::endl;
                MPI_Recv(&start_pos_rec, 1, MPI_INT, i, 1, MPI_COMM_WORLD, &Stat);

                if (start_pos_rec > -1) {
                    vert_cuts[i][j]->parent = vert_cuts[i][start_pos_rec];

                    std::cout<<"\tparent is: "<<vert_cuts[i][start_pos_rec]->name<<std::endl;
                }
                else {
                    std::cout<<"\treceived no parent!"<<std::endl;
                }
            }
        }
    }
    else {
        for (int i = 0; i < vert_cuts[rank].size(); i++) {
            if (vert_cuts[rank][i]->parent) {
                std::cout<<"vert_cuts["<<rank<<"]["<<i<<"] has a parent - "<<
                            vert_cuts[rank][i]->parent->name<<std::endl;
                for (int j = 0; j < vert_cuts[rank].size(); j++) {
                    if (vert_cuts[rank][i]->parent->name == vert_cuts[rank][j]->name) {
                        start_pos_send = j;
                        std::cout<<"start_pos_send: "<<start_pos_send<<std::endl;
                        MPI_Send(&start_pos_send, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
                        break;
                    }
                }
            }
            else {
                std::cout<<"vert_cuts["<<rank<<"]["<<i<<"] has no parent!"<<std::endl;
                start_pos_send = -1;
                MPI_Send(&start_pos_send, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
            }
        }
    }

    if (rank == 0) {
        for (int i = 1; i < n_processes; i++) {
            std::cout<<"vert_cuts["<<i<<"].size(): "<<vert_cuts[i].size()<<std::endl;
            for (int j = 0; j < vert_cuts[i].size(); j++) {
                MPI_Recv(&end_pos_rec, max, MPI_INT, i, 0, MPI_COMM_WORLD, &Stat);

                for (int k = 1; k < end_pos_rec[0] + 1; k++) {
                    vert_cuts[i][j]->children.push_back(vert_cuts[i][end_pos_rec[k]]);

                    std::cout<<"vert_cuts["<<i<<"]["<<j<<"]->children gets: "<<
                                            vert_cuts[i][end_pos_rec[k]]->name<<std::endl;
                }
            }
        }
    }
    else {
        for (int i = 0; i < vert_cuts[rank].size(); i++) {
            end_pos_send[0] = vert_cuts[rank][i]->children.size();
            std::cout<<"vert_cuts["<<rank<<"]["<<i<<"] has "<<
                            vert_cuts[rank][i]->children.size()<<"child(ren), and "<<
                            "end_pos_send[0] is "<<end_pos_send[0]<<std::endl;
            for (int j = 0; j < vert_cuts[rank][i]->children.size(); j++) {
                int name = vert_cuts[rank][i]->children[j]->name;
                std::cout<<"vert_cuts["<<rank<<"]["<<i<<"] has a child - "<<
                                            vert_cuts[rank][i]->children[j]->name<<std::endl;
                for (int k = 0; k < vert_cuts[rank].size(); k++) {
                    if (name == vert_cuts[rank][k]->name) {
                        std::cout<<"sending child "<<vert_cuts[rank][k]->name<<std::endl;
                        end_pos_send[j + 1] = k;
                    }
                }
            }
            for (int j = 0; j < vert_cuts[rank][i]->children.size() + 1; j++) {
                std::cout<<"vert_cuts["<<rank<<"]["<<i<<"] end_pos_send["<<j<<"]: "<<
                                    end_pos_send[j]<<std::endl;
            }
            MPI_Send(&end_pos_send, max, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
    }
}

void traverse(Vertex* vert) { // traverse and print from passed vertex through tree
    std::cout<<vert->name<<std::endl;
    if (vert->children.size() > 0) {
        for (Vertex* child : vert->children) {traverse(child);}
    }
}

void hamiltonian(std::vector<Vertex*>* vert_cuts, int mins[], int n_sectors) {
    // std::cout<<"n_sectors: "<<n_sectors<<std::endl;
    
    for (int i = 1; i < n_sectors; i++) { // linking sectors by their minimum-weighted vertexes
        std::cout<<"at sector "<<i<<std::endl;
        std::cout<<"\tsize of cut: "<<vert_cuts[i].size()<<std::endl;
        for (int j = 0; j < vert_cuts[i].size(); j++) {
            std::cout<<"\tat element "<<j<<std::endl;
            if (vert_cuts[i][j]->parent == nullptr) {
                std::cout<<"\t\tfound an orphan. name: "<<vert_cuts[i][j]->name<<std::endl;
                std::cout<<"\t\ti % (int)sqrt(n_sectors): "<<i % (int)sqrt(n_sectors)<<std::endl;
                if (i % (int)sqrt(n_sectors) > 0 ) { // trying to link proximal sectors
                    std::cout<<"\t\tmins["<<i<<" - 1]: "<<mins[i - 1]<<std::endl;
                    vert_cuts[i][j]->parent = vert_cuts[i - 1][mins[i - 1]];
                    vert_cuts[i - 1][mins[i - 1]]->children.push_back(vert_cuts[i][j]);
                } else {
                    std::cout<<"\t\tmins["<<i<<" - (int)sqrt(n_sectors)]: "<<
                                        mins[i - (int)sqrt(n_sectors)]<<std::endl;
                    vert_cuts[i][j]->parent = vert_cuts[i - (int)sqrt(n_sectors)][mins[i - (int)sqrt(n_sectors)]];
                    vert_cuts[i - (int)sqrt(n_sectors)][mins[i - (int)sqrt(n_sectors)]]->
                                    children.push_back(vert_cuts[i][j]);
                }
                break;
            }
        }
    }
    for (int i = 0; i < n_sectors; i++) {
        for (int j = 0; j < vert_cuts[i].size(); j++) {
            std::cout<<"vert_cut["<<i<<"]: "<<std::endl;
            std::cout<<"\tvertex["<<j<<"]: "<<std::endl;
            std::cout<<"\tname: "<<vert_cuts[i][j]->name<<std::endl;
            if (vert_cuts[i][j]->parent) {std::cout<<"\tparent: "<<vert_cuts[i][j]->parent->name<<std::endl;}
            for (int k = 0; k < vert_cuts[i][j]->children.size(); k++) {
                std::cout<<"\tchild["<<k<<"]: "<<vert_cuts[i][j]->children[k]->name<<std::endl;
            }
        }
    }
    std::cout<<"Start traversal"<<std::endl;
    // vert_cuts[0][0] has no parent, must start there
    traverse(vert_cuts[0][0]);
}

int main(int argc, char *argv[]) {
    std::vector<Vertex*> vertexes;
    std::vector<Vertex> verts;
    for (int i = 1; i < argc - 1; i+=3) {
        Vertex v = {std::stof(argv[i + 2], nullptr),
                            i / 3,
                            {std::stof(argv[i], nullptr), std::stof(argv[i + 1], nullptr)}
                            };
        verts.push_back(v);
    }
    for (int i = 0; i < verts.size(); i++) {
        vertexes.push_back(&verts[i]);
    }
    MPI_Init(&argc, &argv);
    int n_processes, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &n_processes);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int rows = (int)sqrt(n_processes);

    if ((float)rows < sqrt(n_processes) && rank == 0) {
        std::cout<<"non-square (1, 4, 9, 16, ...) # of processes, use "<<
                                rows * rows<<" processes instead."<<std::endl;
        exit(0);
    }

    float size = 500 / rows; // hard coded coordinate ranges
                                            // would be better with a range finding function

    std::vector<int> vert_cut_names[n_processes];
    std::vector<Vertex*> vert_cuts[n_processes];

    for (int i = 0; i < n_processes; i++) {
        for (int j = 0; j < vertexes.size(); j++) {
            if ((vertexes[j]->coords[0] > i / rows * size)
                && (vertexes[j]->coords[0] < (i / rows + 1) * size)
                && (vertexes[j]->coords[1] > i % rows * size)
                && (vertexes[j]->coords[1] <  ((i % rows + 1) * size))) {
                    vert_cut_names[i].push_back(vertexes[j]->name);
                    vert_cuts[i].push_back(vertexes[j]);
            }
        }
    }

    int min;
    int mins[n_processes];
    int rec_buff[vert_cuts[rank].size()];

    MPI_Status Stat;
    if (rank == 0) {
        for (int i = 1; i < n_processes; i++) {
            int send_buff[vert_cuts[i].size()];
            std::copy(vert_cut_names[i].begin(), vert_cut_names[i].end(), send_buff);

            MPI_Send(&send_buff, vert_cuts[i].size(), MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }
    else { 
            MPI_Recv(&rec_buff, vert_cuts[rank].size(), MPI_INT, 0, 0, MPI_COMM_WORLD, &Stat);
    }
    
    if (rank == 0) {
        std::copy(vert_cut_names[0].begin(), vert_cut_names[0].end(), rec_buff);
    }
    
    min = MST(vertexes, rec_buff, sizeof(rec_buff) / sizeof(rec_buff[0]));
    MPI_Gather(&min, 1, MPI_INT, mins, 1, MPI_INT, 0, MPI_COMM_WORLD);
    gather_verts(vert_cuts, n_processes);
    // if (rank == 0) {std::cout<<"got to c!"<<std::endl;}
    MPI_Finalize();
    // if (rank == 0) {std::cout<<"got to d!"<<std::endl;

    if (rank == 0) {
        for (int i = 0; i < vertexes.size(); i++) {
            std::cout<<"vertexes["<<i<<"]: "<<std::endl;
            std::cout<<"\tname: "<<vertexes[i]->name<<std::endl;
            if (vertexes[i]->parent) {std::cout<<"\tparent: "<<vertexes[i]->parent->name<<std::endl;}
            for (int j = 0; j <vertexes[i]->children.size(); j++) {
                std::cout<<"\tchild["<<j<<"]: "<<vertexes[i]->children[j]->name<<std::endl;
            }
        }
        // std::cout<<"got to e!"<<std::endl;
        hamiltonian(vert_cuts, mins, n_processes);
        std::cout<<"Finished!"<<std::endl;
    }

    return 0;
}